# Ghidra script: apply Dino Crisis 2 PC labels from dc2_labels.csv.
# Run from Script Manager (Jython or PyGhidra). Asks for the CSV file.
# Functions get created if missing and renamed; data gets a primary label.
# @category DC2
import csv

from ghidra.program.model.symbol import SourceType

f = askFile("dc2_labels.csv", "Apply")
st = currentProgram.getSymbolTable()
fm = currentProgram.getFunctionManager()
done = 0
with open(f.getAbsolutePath()) as fh:
    for row in csv.DictReader(fh):
        addr = toAddr(int(row["address"], 16))
        name = row["name"]
        comment = "[%s/%s] %s" % (row["category"], row["confidence"], row["notes"])
        if row["kind"] == "func":
            fn = fm.getFunctionAt(addr)
            if fn is None:
                disassemble(addr)
                fn = createFunction(addr, name)
            if fn is not None:
                fn.setName(name, SourceType.USER_DEFINED)
                fn.setComment(comment)
        else:
            st.createLabel(addr, name, SourceType.USER_DEFINED).setPrimary()
            setPlateComment(addr, comment)
        done += 1
print("applied %d labels" % done)
