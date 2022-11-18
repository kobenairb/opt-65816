#!/usr/bin/env python

import sys
import re
import os

verbose = not os.getenv("OPT816_QUIET")
# open the assembler file and put lines in array text
text_raw = open(sys.argv[1], "r").readlines()
text = [l.strip() for l in text_raw if not l.startswith(";")]

# find .bss section symbols
bss = []
bsson = False
for l in text:
    if l == '.RAMSECTION ".bss" BANK $7e SLOT 2':
        bsson = True
        continue
    if l == ".ENDS":
        bsson = False
    if bsson:
        bss += [l.split(" ")[0]]
        # print(f"bss {bss}")

# checks if the line alters the control flow
def is_control(line):
    return bool(len(line) > 0 and line[0] in "jb+-" or line.endswith(":"))


def changes_accu(line):
    return bool(
        line[2] == "a"
        and line[:3] not in ["pha", "sta"]
        or len(line) == 5
        and line.endswith(" a")
    )


totalopt = 0  # total number of optimizations performed
opted = -1  # have we optimized in this pass?
opass = 0  # optimization pass counter
storetopseudo = re.compile("st([axyz]).b tcc__([rf][0-9]*h?)$")
storexytopseudo = re.compile("st([xy]).b tcc__([rf][0-9]*h?)$")
storeatopseudo = re.compile("sta.b tcc__([rf][0-9]*h?)$")


opass += 1
if verbose:
    sys.stderr.write(f"optimization pass {opass}: ")
opted = 0  # no optimizations performed
text_opt = []  # optimized code array, will be filled in during this pass
i = 0
while i < len(text):
    if text[i].startswith("st"):
        # stores (accu/x/y/zero) to pseudo-registers
        r = storetopseudo.match(text[i])
        if r:
            # eliminate redundant stores
            doopt = False
            for j in range(i + 1, min(len(text), i + 30)):
                r1 = re.match("st([axyz]).b tcc__" + r.groups()[1] + "$", text[j])
                if r1:
                    print(f"[USECASE #1] {j}: {text[j]}")
                    doopt = True  # another store to the same pregister
                    break
                if text[j].startswith("jsr.l ") and not text[j].startswith(
                    "jsr.l tcc__"
                ):
                    print(f"[USECASE #2] {j}: {text[j]}")
                    doopt = True  # before function call (will be clobbered anyway)
                    break
                # cases in which we don't pursue optimization further
                if is_control(text[j]) or ("tcc__" + r.groups()[1]) in text[j]:
                    print(f"[USECASE #3] {j}: {text[j]}")
                    break  # branch or other use of the preg
                if (
                    r.groups()[1].endswith("h")
                    and ("[tcc__" + r.groups()[1].rstrip("h")) in text[j]
                ):
                    print(f"[USECASE #4] {j}: {text[j]}")
                    break  # use as a pointer
            if doopt:
                i += 1  # skip redundant store
                opted += 1
                continue

        # stores (x/y) to pseudo-registers
        r = storexytopseudo.match(text[i])
        if r:
            # store hwreg to preg, push preg, function call -> push hwreg, function call
            if text[i + 1] == "pei (tcc__" + r.groups()[1] + ")" and text[
                i + 2
            ].startswith("jsr.l "):
                print(f"[USECASE #5] {i + 1}: {text[i + 1]}")
                text_opt += ["ph" + r.groups()[0]]
                i += 2
                opted += 1
                continue
            # store hwreg to preg, push preg -> store hwreg to preg, push hwreg (shorter)
            if text[i + 1] == "pei (tcc__" + r.groups()[1] + ")":
                print(f"[USECASE #6] {i+1}: {text[i + 1]}")
                text_opt += [text[i]]
                text_opt += ["ph" + r.groups()[0]]
                i += 2
                opted += 1
                continue
            # store hwreg to preg, load hwreg from preg -> store hwreg to preg, transfer hwreg/hwreg (shorter)
            if (
                text[i + 1] == "lda.b tcc__" + r.groups()[1]
                or text[i + 1] == "lda.b tcc__" + r.groups()[1] + " ; DON'T OPTIMIZE"
            ):
                print(f"[USECASE #7] {i+1}: {text[i +1]}")
                text_opt += [text[i]]
                text_opt += [
                    "t" + r.groups()[0] + "a"
                ]  # FIXME: shouldn't this be marked as DON'T OPTIMIZE again?
                i += 2
                opted += 1
                continue

        # stores (accu only) to pseudo-registers
        r = storeatopseudo.match(text[i])
        if r:
            # store preg followed by load preg
            if text[i + 1] == "lda.b tcc__" + r.groups()[0]:
                print(f"[USECASE #8] {i+1}: {text[i +1]}")
                text_opt += [text[i]]  # keep store
                i += 2  # omit load
                opted += 1
                continue
            # store preg followed by load preg with ldx/ldy in between
            if (
                text[i + 1].startswith("ldx") or text[i + 1].startswith("ldy")
            ) and text[i + 2] == "lda.b tcc__" + r.groups()[0]:
                print(f"[USECASE #9] {i+1}: {text[i +1]}")
                text_opt += [text[i]]  # keep store
                text_opt += [text[i + 1]]
                i += 3  # omit load
                opted += 1
                continue
            # store accu to preg, push preg, function call -> push accu, function call
            if text[i + 1] == "pei (tcc__" + r.groups()[0] + ")" and text[
                i + 2
            ].startswith("jsr.l "):
                print(f"[USECASE #10] {i+1}: {text[i +1]}")
                text_opt += ["pha"]
                i += 2
                opted += 1
                continue
            # store accu to preg, push preg -> store accu to preg, push accu (shorter)
            if text[i + 1] == "pei (tcc__" + r.groups()[0] + ")":
                print(f"[USECASE #11] {i+1}: {text[i +1]}")
                text_opt += [text[i]]
                text_opt += ["pha"]
                i += 2
                opted += 1
                continue
            # store accu to preg1, push preg2, push preg1 -> store accu to preg1, push preg2, push accu
            elif (
                text[i + 1].startswith("pei ")
                and text[i + 2] == "pei (tcc__" + r.groups()[0] + ")"
            ):
                print(f"[USECASE #12] {i+1}: {text[i +1]}")
                text_opt += [text[i + 1]]
                text_opt += [text[i]]
                text_opt += ["pha"]
                i += 3
                opted += 1
                continue

            # convert incs/decs on pregs incs/decs on hwregs
            cont = False
            for crem in "inc", "dec":
                if text[i + 1] == crem + ".b tcc__" + r.groups()[0]:
                    # store to preg followed by crement on preg
                    print(f"[USECASE #13] {i+1}: {text[i+1]}")
                    if text[i + 2] == crem + ".b tcc__" + r.groups()[0] and text[
                        i + 3
                    ].startswith("lda"):
                        # store to preg followed by two crements on preg
                        # increment the accu first, then store it to preg
                        print(f"[USECASE #14] {i+2}: {text[i+2]}")
                        text_opt += [
                            crem + " a",
                            crem + " a",
                            "sta.b tcc__" + r.groups()[0],
                        ]
                        # a subsequent load can be omitted (the right value is already in the accu)
                        if text[i + 3] == "lda.b tcc__" + r.groups()[0]:
                            print(f"[USECASE #15] {i+3}: {text[i+3]}")
                            i += 4
                        else:
                            print(f"[USECASE #16] {i}: {text[i]}")
                            i += 3
                        opted += 1
                        cont = True
                        break
                    elif text[i + 2].startswith(
                        "lda"
                    ):  # text[i+2] == 'lda.b tcc__' + r.groups()[0]:
                        # same thing with only one crement (FIXME: there should be a more clever way to do this...)
                        print(f"[USECASE #17] {i+2}: {text[i+2]}")
                        text_opt += [crem + " a", "sta.b tcc__" + r.groups()[0]]
                        if text[i + 2] == "lda.b tcc__" + r.groups()[0]:
                            print(f"[USECASE #18] {i+2}: {text[i+2]}")
                            i += 3
                        else:
                            print(f"[USECASE #19] {i}: {text[i]}")
                            i += 2
                        opted += 1
                        cont = True
                        break
            if cont:
                continue

            r1 = re.match("lda.b tcc__([rf][0-9]*)", text[i + 1])
            if r1:
                print(f"[USECASE #20] {i+1}: {text[i+1]}")
                if text[i + 2][:3] in ["and", "ora"]:
                    print(f"[USECASE #21] {i+2}: {text[i+2]}")
                    # store to preg1, load from preg2, and/or preg1 -> store to preg1, and/or preg2
                    if text[i + 2][3:] == ".b tcc__" + r.groups()[0]:
                        print(f"[USECASE #22] {i+2}: {text[i+2]}")
                        text_opt += [text[i]]  # store
                        text_opt += [text[i + 2][:3] + ".b tcc__" + r1.groups()[0]]
                        i += 3
                        opted += 1
                        continue

            # store to preg, switch to 8 bits, load from preg => skip the load
            if (
                text[i + 1] == "sep #$20"
                and text[i + 2] == "lda.b tcc__" + r.groups()[0]
            ):
                print(f"[USECASE #23] {i+2}: {text[i+2]}")
                text_opt += [text[i]]
                text_opt += [text[i + 1]]
                i += 3  # skip load
                opted += 1
                continue

            # two stores to preg without control flow or other uses of preg => skip first store
            if (
                not is_control(text[i + 1])
                and not ("tcc__" + r.groups()[0]) in text[i + 1]
            ):
                print(f"[USECASE #24] {i+1}: {text[i+1]}")
                if text[i + 2] == text[i]:
                    print(f"[USECASE #25] {i+2}: {text[i+2]}")
                    text_opt += [text[i + 1]]
                    text_opt += [text[i + 2]]
                    i += 3  # skip first store
                    opted += 1
                    continue

            # store hwreg to preg, load hwreg from preg -> store hwreg to preg, transfer hwreg/hwreg (shorter)
            r1 = re.match("ld([xy]).b tcc__" + r.groups()[0], text[i + 1])
            if r1:
                print(f"[USECASE #26] {i+1}: {text[i+1]}")
                text_opt += [text[i]]
                text_opt += ["ta" + r1.groups()[0]]
                i += 2
                opted += 1
                continue

            # store accu to preg then load accu from preg, with something in-between that does not alter
            # control flow or touch accu or preg => skip load
            if not (
                is_control(text[i + 1])
                or changes_accu(text[i + 1])
                or "tcc__" + r.groups()[0] in text[i + 1]
            ):
                print(f"[USECASE #27] {i+1}: {text[i+1]}")
                if text[i + 2] == "lda.b tcc__" + r.groups()[0]:
                    print(f"[USECASE #28] {i+1}: {text[i+1]}")
                    text_opt += [text[i]]
                    text_opt += [text[i + 1]]
                    i += 3  # skip load
                    opted += 1
                    continue

            # store preg1, clc, load preg2, add preg1 -> store preg1, clc, add preg2
            if text[i + 1] == "clc":
                print(f"[USECASE #29] {i+1}: {text[i+1]}")
                r1 = re.match("lda.b tcc__(r[0-9]*)", text[i + 2])
                if r1 and text[i + 3] == "adc.b tcc__" + r.groups()[0]:
                    print(f"[USECASE #30] {i+3}: {text[i+3]}")
                    text_opt += [text[i]]
                    text_opt += [text[i + 1]]
                    text_opt += ["adc.b tcc__" + r1.groups()[0]]
                    i += 4  # skip load
                    opted += 1
                    continue

            # store accu to preg, asl preg => asl accu, store accu to preg
            # FIXME: is this safe? can we rely on code not making assumptions about the contents of the accu
            # after the shift?
            if text[i + 1] == "asl.b tcc__" + r.groups()[0]:
                print(f"[USECASE #31] {i+1}: {text[i+1]}")
                text_opt += ["asl a"]
                text_opt += [text[i]]
                i += 2
                opted += 1
                continue

        r = re.match("sta (.*),s$", text[i])
        if r:
            if text[i + 1] == "lda " + r.groups()[0] + ",s":
                print(f"[USECASE #32] {i+1}: {text[i+1]}")
                text_opt += [text[i]]
                i += 2  # omit load
                opted += 1
                continue
    # end startswith('st')

    if text[i].startswith("ld"):
        print(f"[USECASE #33] {i}: {text[i]}")
        r = re.match("ldx #0", text[i])
        if r:
            print(f"[USECASE #34] {i}: {text[i]}")
            r1 = re.match("lda.l (.*),x$", text[i + 1])
            if r1 and not text[i + 3].endswith(",x"):
                print(f"[USECASE #33] {i+3}: {text[i+3]}")
                text_opt += ["lda.l " + r1.groups()[0]]
                i += 2
                opted += 1
                continue
            elif r1:
                print(f"[USECASE #36] {i}: {text[i]}")
                text_opt += ["lda.l " + r1.groups()[0]]
                text_opt += [text[i + 2]]
                text_opt += [text[i + 3].replace(",x", "")]
                i += 4
                opted += 1
                continue

        if (
            text[i].startswith("lda.w #")
            and text[i + 1] == "sta.b tcc__r9"
            and text[i + 2].startswith("lda.w #")
            and text[i + 3] == "sta.b tcc__r9h"
            and text[i + 4] == "sep #$20"
            and text[i + 5].startswith("lda.b ")
            and text[i + 6] == "sta.b [tcc__r9]"
            and text[i + 7] == "rep #$20"
        ):
            print(f"[USECASE #37] {i}: {text[i]}")
            text_opt += ["sep #$20"]
            text_opt += [text[i + 5]]
            text_opt += [
                "sta.l " + str(int(text[i + 2][7:]) * 65536 + int(text[i][7:]))
            ]
            text_opt += ["rep #$20"]
            i += 8
            opted += 1
            continue

        if text[i] == "lda.w #0":
            print(f"[USECASE #38] {i}: {text[i]}")
            if text[i + 1].startswith("sta.b ") and text[i + 2].startswith("lda"):
                print(f"[USECASE #39] {i+1}: {text[i+1]}")
                text_opt += [text[i + 1].replace("sta.", "stz.")]
                i += 2
                opted += 1
                continue
        elif text[i].startswith("lda.w #"):
            print(f"[USECASE #40] {i}: {text[i]}")
            if (
                text[i + 1] == "sep #$20"
                and text[i + 2].startswith("sta ")
                and text[i + 3] == "rep #$20"
                and text[i + 4].startswith("lda")
            ):
                print(f"[USECASE #41] {i+1}: {text[i+1]}")
                text_opt += [
                    "sep #$20",
                    text[i].replace("lda.w", "lda.b"),
                    text[i + 2],
                    text[i + 3],
                ]
                i += 4
                opted += 1
                continue

        if (
            text[i].startswith("lda.b")
            and not is_control(text[i + 1])
            and not "a" in text[i + 1]
            and text[i + 2].startswith("lda.b")
        ):
            print(f"[USECASE #42] {i}: {text[i]}")
            text_opt += [text[i + 1], text[i + 2]]
            i += 3
            opted += 1
            continue

        # don't write preg high back to stack if it hasn't been updated
        if (
            text[i + 1].endswith("h")
            and text[i + 1].startswith("sta.b tcc__r")
            and text[i].startswith("lda ")
            and text[i].endswith(",s")
        ):
            print(f"[USECASE #43] {i+1}: {text[i+1]}")
            local = text[i][4:]
            reg = text[i + 1][6:]
            # lda stack ; store high preg ; ... ; load high preg ; sta stack
            j = i + 2
            while j < len(text) - 2 and not is_control(text[j]) and not reg in text[j]:
                print(f"[USECASE #44] {j}: {text[j]}")
                j += 1
            if text[j] == "lda.b " + reg and text[j + 1] == "sta " + local:
                while i < j:
                    text_opt += [text[i]]
                    i += 1
                i += 2  # skip load high preg ; sta stack
                opted += 1
                continue

        # reorder copying of 32-bit value to preg if it looks as if that could
        # allow further optimization
        # looking for
        #   lda something
        #   sta.b tcc_rX
        #   lda something
        #   sta.b tcc_rYh
        #   ...tcc_rX...
        if text[i].startswith("lda") and text[i + 1].startswith("sta.b tcc__r"):
            reg = text[i + 1][6:]
            print(f"[USECASE #45] {i}: {text[i]}")
            if (
                not reg.endswith("h")
                and text[i + 2].startswith("lda")
                and not text[i + 2].endswith(reg)
                and text[i + 3].startswith("sta.b tcc__r")
                and text[i + 3].endswith("h")
                and text[i + 4].endswith(reg)
            ):
                print(f"[USECASE #46] {i+2}: {text[i+2]}")
                text_opt += [text[i + 2], text[i + 3]]
                text_opt += [text[i], text[i + 1]]
                i += 4
                # this is not an optimization per se, so we don't count it
                continue

        # compare optimizations inspired by optimore
        # These opts simplify compare operations, which are monstrous because
        # they have to take the long long case into account.
        # We try to detect those cases by checking if a tya follows the
        # comparison (not sure if this is reliable, but it passes the test suite)
        if (
            text[i] == "ldx #1"
            and text[i + 1].startswith("lda.b tcc__")
            and text[i + 2] == "sec"
            and text[i + 3].startswith("sbc #")
            and text[i + 4] == "tay"
            and text[i + 5] == "beq +"
            and text[i + 6] == "dex"
            and text[i + 7] == "+"
            and text[i + 8].startswith("stx.b tcc__")
            and text[i + 9] == "txa"
            and text[i + 10] == "bne +"
            and text[i + 11].startswith("brl ")
            and text[i + 12] == "+"
            and text[i + 13] != "tya"
        ):
            print(f"[USECASE #47] {i}: {text[i]}")
            text_opt += [text[i + 1]]
            text_opt += ["cmp #" + text[i + 3][5:]]
            text_opt += [text[i + 5]]
            text_opt += [text[i + 11]]  # brl
            text_opt += [text[i + 12]]  # +
            i += 13
            opted += 1
            continue

        if (
            text[i] == "ldx #1"
            and text[i + 1] == "sec"
            and text[i + 2].startswith("sbc #")
            and text[i + 3] == "tay"
            and text[i + 4] == "beq +"
            and text[i + 5] == "dex"
            and text[i + 6] == "+"
            and text[i + 7].startswith("stx.b tcc__")
            and text[i + 8] == "txa"
            and text[i + 9] == "bne +"
            and text[i + 10].startswith("brl ")
            and text[i + 11] == "+"
            and text[i + 12] != "tya"
        ):
            print(f"[USECASE #48] {i}: {text[i]}")
            text_opt += ["cmp #" + text[i + 2][5:]]
            text_opt += [text[i + 4]]
            text_opt += [text[i + 10]]  # brl
            text_opt += [text[i + 11]]  # +
            i += 12
            opted += 1
            continue

        if (
            text[i] == "ldx #1"
            and text[i + 1].startswith("lda.b tcc__r")
            and text[i + 2] == "sec"
            and text[i + 3].startswith("sbc.b tcc__r")
            and text[i + 4] == "tay"
            and text[i + 5] == "beq +"
            and text[i + 6] == "bcs ++"
            and text[i + 7] == "+ dex"
            and text[i + 8] == "++"
            and text[i + 9].startswith("stx.b tcc__r")
            and text[i + 10] == "txa"
            and text[i + 11] == "bne +"
            and text[i + 12].startswith("brl ")
            and text[i + 13] == "+"
            and text[i + 14] != "tya"
        ):
            print(f"[USECASE #49] {i}: {text[i]}")
            text_opt += [text[i + 1]]
            text_opt += ["cmp.b " + text[i + 3][6:]]
            text_opt += [text[i + 5]]
            text_opt += ["bcc +"]
            text_opt += ["brl ++"]
            text_opt += ["+"]
            text_opt += [text[i + 12]]
            text_opt += ["++"]
            i += 14
            opted += 1
            continue

        if (
            text[i] == "ldx #1"
            and text[i + 1] == "sec"
            and text[i + 2].startswith("sbc.w #")
            and text[i + 3] == "tay"
            and text[i + 4] == "bvc +"
            and text[i + 5] == "eor #$8000"
            and text[i + 6] == "+"
            and text[i + 7] == "bmi +++"
            and text[i + 8] == "++"
            and text[i + 9] == "dex"
            and text[i + 10] == "+++"
            and text[i + 11].startswith("stx.b tcc__r")
            and text[i + 12] == "txa"
            and text[i + 13] == "bne +"
            and text[i + 14].startswith("brl ")
            and text[i + 15] == "+"
            and text[i + 16] != "tya"
        ):
            print(f"[USECASE #50] {i}: {text[i]}")
            text_opt += [text[i + 1]]
            text_opt += [text[i + 2]]
            text_opt += [text[i + 4]]
            text_opt += ["eor #$8000"]
            text_opt += ["+"]
            text_opt += ["bmi +"]
            text_opt += [text[i + 14]]
            text_opt += ["+"]
            i += 16
            opted += 1
            continue

        if (
            text[i] == "ldx #1"
            and text[i + 1].startswith("lda.b tcc__r")
            and text[i + 2] == "sec"
            and text[i + 3].startswith("sbc.b tcc__r")
            and text[i + 4] == "tay"
            and text[i + 5] == "bvc +"
            and text[i + 6] == "eor #$8000"
            and text[i + 7] == "+"
            and text[i + 8] == "bmi +++"
            and text[i + 9] == "++"
            and text[i + 10] == "dex"
            and text[i + 11] == "+++"
            and text[i + 12].startswith("stx.b tcc__r")
            and text[i + 13] == "txa"
            and text[i + 14] == "bne +"
            and text[i + 15].startswith("brl ")
            and text[i + 16] == "+"
            and text[i + 17] != "tya"
        ):
            print(f"[USECASE #51] {i}: {text[i]}")
            text_opt += [text[i + 1]]
            text_opt += [text[i + 2]]
            text_opt += [text[i + 3]]
            text_opt += [text[i + 5]]
            text_opt += [text[i + 6]]
            text_opt += ["+"]
            text_opt += ["bmi +"]
            text_opt += [text[i + 15]]
            text_opt += ["+"]
            i += 17
            opted += 1
            continue

        if (
            text[i] == "ldx #1"
            and text[i + 1] == "sec"
            and text[i + 2].startswith("sbc.b tcc__r")
            and text[i + 3] == "tay"
            and text[i + 4] == "bvc +"
            and text[i + 5] == "eor #$8000"
            and text[i + 6] == "+"
            and text[i + 7] == "bmi +++"
            and text[i + 8] == "++"
            and text[i + 9] == "dex"
            and text[i + 10] == "+++"
            and text[i + 11].startswith("stx.b tcc__r")
            and text[i + 12] == "txa"
            and text[i + 13] == "bne +"
            and text[i + 14].startswith("brl ")
            and text[i + 15] == "+"
            and text[i + 16] != "tya"
        ):
            print(f"[USECASE #52] {i}: {text[i]}")
            text_opt += [text[i + 1]]
            text_opt += [text[i + 2]]
            text_opt += [text[i + 4]]
            text_opt += [text[i + 5]]
            text_opt += ["+"]
            text_opt += ["bmi +"]
            text_opt += [text[i + 14]]
            text_opt += ["+"]
            i += 16
            opted += 1
            continue

    # # end startswith('ld')

    if text[i] == "rep #$20" and text[i + 1] == "sep #$20":
        print(f"[USECASE #53] {i}: {text[i]}")
        i += 2
        opted += 1
        continue

    if (
        text[i] == "sep #$20"
        and text[i + 1].startswith("lda #")
        and text[i + 2] == "pha"
        and text[i + 3].startswith("lda #")
        and text[i + 4] == "pha"
    ):
        print(f"[USECASE #54] {i}: {text[i]}")
        text_opt += [
            "pea.w ("
            + text[i + 1].split("#")[1]
            + " * 256 + "
            + text[i + 3].split("#")[1]
            + ")"
        ]
        text_opt += [text[i]]
        i += 5
        opted += 1
        continue

    r = re.match("adc #(.*)$", text[i])
    if r:
        print(f"[USECASE #55] {i}: {text[i]}")
        r1 = re.match("sta.b (tcc__[fr][0-9]*)$", text[i + 1])
        if r1:
            print(f"[USECASE #56] {i+1}: {text[i+1]}")
            if (
                text[i + 2] == "inc.b " + r1.groups()[0]
                and text[i + 3] == "inc.b " + r1.groups()[0]
            ):
                print(f"[USECASE #57] {i+2}: {text[i+2]}")
                text_opt += ["adc #" + r.groups()[0] + " + 2"]
                text_opt += [text[i + 1]]
                i += 4
                opted += 1
                continue

    if text[i][:6] in ["lda.l ", "sta.l "]:
        cont = False
        for b in bss:
            if text[i][2:].startswith("a.l " + b + " "):
                print(f"[USECASE #58] {i}: {text[i]}")
                text_opt += [
                    text[i].replace("lda.l", "lda.w").replace("sta.l", "sta.w")
                ]
                i += 1
                opted += 1
                cont = True
                break
        if cont:
            continue

    # if text[i].startswith("jmp.w ") or text[i].startswith("bra __"):
    #     j = i + 1
    #     cont = False
    #     while j < len(text) and text[j].endswith(":"):
    #         if text[i].endswith(text[j][:-1]):
    #             # redundant branch, discard it
    #             i += 1
    #             opted += 1
    #             cont = True
    #             break
    #         j += 1
    #     if cont:
    #         continue

    # if text[i].startswith("jmp.w "):
    #     # worst case is a 4-byte instruction, so if the jump target is closer
    #     # than 32 instructions, we can safely substitute a branch
    #     label = text[i][6:] + ":"
    #     cont = False
    #     for lpos in range(max(0, i - 32), min(len(text), i + 32)):
    #         if text[lpos] == label:
    #             text_opt += [text[i].replace("jmp.w", "bra")]
    #             i += 1
    #             opted += 1
    #             cont = True
    #             break
    #     if cont:
    #         continue

    # text_opt += [text[i]]
    i += 1
text = text_opt
if verbose:
    sys.stderr.write(f"{opted} optimizations performed\n")
totalopt += opted

print("\n\n______________[ASM CODE]_________________")
for l in text_opt:
    print(l)
if verbose:
    sys.stderr.write(f"{totalopt} optimizations performed in total\n")
