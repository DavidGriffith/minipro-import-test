#!/usr/bin/env python3

"""Create detailed testing XML fragments for various ICs, mostly in
the 74xx family.

"""

import itertools
import os
import re
import sys
import pprint


db = dict()
_name = None


def start_entry(name, pins, voltage="5V", **kwargs):
    global _name
    _name = name
    entry = dict(name=name, pins=pins, voltage=voltage)
    entry.update(kwargs)
    entry['vectors'] = []
    db[name] = entry


def addvec(vector):
    db[_name]['vectors'].append(vector)


def entry_header(entry):
    return '      <ic name="{name}" type="5" voltage="{voltage}" pins="{pins}">'.format(**entry)


def entry_body(entry):
    res = ''
    for n, vector in enumerate(entry['vectors']):
        res += '        <vector id="{:>04d}"> {} </vector>\n'.format(n, vector)
    return res.rstrip("\n")


def emit_entry(name):
    entry = db[name]
    print(entry_header(entry))
    print(entry_body(entry))
    print('      </ic>')


###############################################################################
#
# HEX INVERTERS AND BUFFERS
#
###############################################################################

def std_inverter(name, high="H", low="L", pins=14,
                 voltage="5V"):
    """Generate a standard 7404-like IC."""

    gates = (pins - 2) // 2
    assert pins & 1 == 0, "pins should be an even number"
    assert gates & 1 == 0, "gates should be an even number"

    start_entry(name, pins, voltage=voltage)

    def f(x):
        """Invert logic"""
        if x == "0":
            return high
        elif x == "1":
            return low
        return "X"

    for data in itertools.product("01", repeat=gates):
        # Half the vector is on the left side (input pin, then output
        # pin). The other half is on the right side (output pin, then
        # input pin).

        vector = " ".join(x + " " + f(x) for x in data[:gates//2])
        vector += " G "
        vector += " ".join(f(x) + " " + x for x in data[gates//2:])
        vector += " V"
        addvec(vector)


def std_buffer(name, high="H", low="L", **kwargs):
    return std_inverter(name,
                        high=low,
                        low=high, **kwargs)


std_inverter("7404")                       # Hex inverter
std_inverter("7405(NonStandard)")          # Hex inverter (push-pull, non-standard)
std_inverter("7405(OC/OD)", high="Z")      # Hex inverter, Open Collector/Drain
std_inverter("7406(NonStandard)")          # Hex inverter (push-pull, non-standard)
std_inverter("7406(OC/OD)", high="Z")      # Hex inverter, Open Collector/Drain

std_buffer("7407(NonStandard)")            # Hex buffer (push-pull, non-standard)
std_buffer("7407(OC/OD)", low="Z")         # Hex buffer, Open Collector/Drain

std_inverter("7414")                       # Hex inverter with Schmitt trigger


###############################################################################
#
# QUAD LOGIC GATES
#
###############################################################################

# Logic gate table
GATES = {         # NAND  AND  NOR   OR  XOR
    ("0", "0"):    [ "H", "L", "H", "L", "L" ],
    ("0", "1"):    [ "H", "L", "L", "H", "H" ],
    ("1", "0"):    [ "H", "L", "L", "H", "H" ],
    ("1", "1"):    [ "L", "H", "L", "H", "L" ],
}

FX_NAND = 0
FX_AND = 1
FX_NOR = 2
FX_OR = 3
FX_XOR = 4


# From https://stackoverflow.com/questions/5389507/iterating-over-every-two-elements-in-a-list
def pairwise(iterable):
    a = iter(iterable)
    return list(zip(a, a))


def std_gate(name, function, pins=14, voltage="5V", reversed=False, oc=False):
    """Generate a standard quad, 2-input gate."""

    gates = (pins - 2) // 3
    inputs = gates * 2
    assert pins & 1 == 0, "pins should be an even number"
    assert gates & 1 == 0, "gates should be an even number"
    assert inputs + gates + 2 == pins, "pin count sanity check failed"

    start_entry(name, pins, voltage=voltage)

    def f(x, y):
        """Implement the logic function."""
        if x in "01" and y in "01":
            val = GATES[(x, y)][function]
            if oc:
                return val.replace("H", "Z")
            return val
        return "X"

    for data in itertools.product("01", repeat=inputs):
        # Half the vector is on the left side (input pins, then output
        # pin). The other half is on the right side (output pin, then
        # input pins).

        data_pairs = pairwise(data)

        if not reversed:
            vector = " ".join("{} {} {}".format(x, y, f(x, y))
                              for x, y in data_pairs[:gates//2])
            vector += " G "
            vector += " ".join("{} {} {}".format(f(x,y), y, x)
                               for x, y in data_pairs[gates//2:])
            vector += " V"
        else:
            # Some gate ICs are flipped because fuck logic.
            vector = " ".join("{} {} {}".format(f(x,y), y, x)
                              for x, y in data_pairs[:gates//2])
            vector += " G "
            vector += " ".join("{} {} {}".format(x, y, f(x,y))
                               for x, y in data_pairs[gates//2:])
            vector += " V"

        addvec(vector)

std_gate("7400", FX_NAND)               # 7400 Quad 2-input NAND
std_gate("7402", FX_NOR, reversed=True) # 7402 Quad 2-input NOR (gates are flipped!)
std_gate("7403", FX_NAND, oc=True)      # 7403 Quad 2-input NAND (open collector)
std_gate("7408", FX_AND)
std_gate("7409", FX_AND, oc=True)       # 7409 Quad 2-input AND (open collector)
std_gate("7432", FX_OR)                 # 7432 Quad 2-input OR (gates are flipped!)
std_gate("7437", FX_NAND)               # 7437 Quad 2-input NAND gated buffers
std_gate("7438", FX_NAND, oc=True)      # 7438 Quad 2-input NAND gated buffers, OC
std_gate("7486", FX_XOR)
#emit_entry("7400")


###############################################################################
#
# OTHER GATES
#
###############################################################################

def fn_multi_AND(inputs, high="H"):
    if "0" in inputs:
        return "L"
    return high


def fn_multi_NAND(inputs, high="H"):
    if "0" in inputs:
        return high
    return "L"
    

def fn_multi_OR(inputs, high="H"):
    if "1" in inputsL:
        return high
    return "L"
    

def fn_multi_NOR(inputs, high="H"):
    if "1" in inputs:
        return "L"
    return "H"


def triplets(iterable):
    a = iter(iterable)
    return list(zip(a, a, a))


def triple_gate(name, function, voltage="5V", od=False):
    """Generate a triple, three-input gate."""

    pins = 14
    gates = 3
    inputs = gates * 3
    assert pins & 1 == 0, "pins should be an even number"
    assert inputs + gates + 2 == pins, "pin count sanity check failed"

    high = "H"
    if od:
        high = "Z"

    start_entry(name, pins, voltage=voltage)

    for data in itertools.product("01", repeat=inputs):
        y1 = function((data[0], data[1], data[8]), high=high)
        y2 = function((data[2], data[3], data[4]), high=high)
        y3 = function((data[5], data[6], data[7]), high=high)

        vector = "{} {} {} {} {} {} G {} {} {} {} {} {} V".format(
            data[0], data[1], data[2], data[3], data[4], y2,
            y3, data[5], data[6], data[7], y1, data[8])
        addvec(vector)


triple_gate("7410", fn_multi_NAND)           # 7410 Triple 3-input NAND
triple_gate("7411", fn_multi_AND)            # 7411 Triple 3-input AND
triple_gate("7412", fn_multi_NAND, od=True)  # 7412 Triple 3-input AND, open drain
triple_gate("7427", fn_multi_NOR)            # 7427 Triple 3-input NOR


def double_gate(name, function, voltage="5V", od=False):
    """Generate a dual, five-input gate."""

    pins = 14
    gates = 2
    inputs = 10
    assert pins & 1 == 0, "pins should be an even number"
    assert inputs + gates + 2 == pins, "pin count sanity check failed"

    high = "H"
    if od:
        high = "Z"

    start_entry(name, pins, voltage=voltage)

    for data in itertools.product("01", repeat=inputs):
        # Half the vector is on the left side (input pins, then output
        # pin). The other half is on the right side (output pin, then
        # input pins).

        y1 = function("".join(data[:5]), high=high)
        y2 = function("".join(data[5:]), high=high)

        vector = "{} {} {} {} {} {} G {} {} {} {} {} {} V".format(
            data[0], data[1], data[2], data[5], y1, y2,
            data[6], data[7], data[8], data[9], data[3], data[4])
        addvec(vector)


double_gate("74260", fn_multi_NOR) # 74260, two 5-input NOR gates


###############################################################################
#
# DEMULTIPLEXERS/DECODERS
#
###############################################################################

def demux_3_to_8(name, voltage="5V", invert=False, oc=False):
    """Generate a three-to-eight decoder."""

    if invert:
        high, low = "L", "H"
    elif oc:
        high, low = "Z", "L"
    else:
        high, low = "H", "L"

    start_entry(name, 16, voltage=voltage)

    for data in itertools.product("01", repeat=6):
        y = high * 8
        if data[3:6] == "001":
            y[int("".join(data[:3]), 2)] = low
        
        vector = "{} {} {} {} {} {} ".format(*data)
        vector += "{} G {} {} {} {} {} {} {} V".format(
            y[7], y[6], y[5], y[4], y[3], y[2], y[1], y[0])
        addvec(vector)


demux_3_to_8("74138")              # 74138: 3-to-8 decoder
demux_3_to_8("74238", invert=True) # 74138: 3-to-8 decoder


###############################################################################
#
# FLIP-FLOPS & LATCHES
#
###############################################################################

def ff_374(name, voltage="5V", invert=False, oc=False):
    """The 74374 octal D-type flip-flop"""

    start_entry(name, 20, voltage=voltage)

    reg = "ZZZZZZZZ"
    for oe in "10":
        for clk in "C10":
            for d in itertools.product("01", repeat=8):

                if clk == "C":  # When clock strobes, data is registered
                    reg = d
                q = "ZZZZZZZZ"  # Output is at high impedance
                if oe == "0":
                    q = reg       # Unless OE is low.
                
                vector = "{} {} {} {} {} {} {} {} {} G ".format(
                    oe, q[0], d[0], d[1], q[1], q[2], d[2], d[3], q[3])
                vector += "{} {} {} {} {} {} {} {} {} V".format(
                    clk, q[4], d[4], d[5], q[5], q[6], d[6], d[7], q[7])

                addvec(vector)


ff_374("74374")                 # 74374 octal D-type flip-flop


###############################################################################
#
# PATCH THE XML FILE
#
###############################################################################

# Prepare for patching
xml_entries = { entry_header(v).strip() : v for k, v in db.items() }

if not os.isatty(sys.stdin.fileno()):
    lines = iter(sys.stdin)
    for line in lines:
        line = line.rstrip("\n")

        if '<ic name="' in line:
            print(line)
            try:
                # See if we have a patched entry
                key = line.strip()
                entry = xml_entries[key]
                print("Updating {}".format(entry['name']), file=sys.stderr, flush=True)
                print(entry_body(entry))
                del(xml_entries[key])
                while True:
                    line = next(lines).rstrip("\n")
                    if '</ic>' in line:
                        print(line)
                        #print("<!-- SOUP ------------------------------------------------------ -->")
                        break
            except KeyError:
                # We don't have a patched value for this. Output the old entry.
                while True:
                    line = next(lines).rstrip("\n")
                    print(line)
                    if '</ic>' in line:
                        break
        else:
            print(line)
else:
    for k, v in db.items():
        print(entry_header(v))
        print(entry_body(v))
        print("      </ic>")

# End of file.
