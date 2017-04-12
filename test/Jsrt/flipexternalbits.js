//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function ASSERT(index, expected, run) {
    if (run != expected) {
        throw new Error("Failed at RUN index " + index);
    }
}

function FLIP(index, obj, unset, bitIndex, expectsThrow) {
    try { WScript.FlipCustomBit(obj, bitIndex, unset); } catch(e) {
        if (expectsThrow) return;
        /*no throw!*/
        ASSERT(index, false, true);
    }

    if (expectsThrow) ASSERT(index, false, true);
}

function CHECK(index, x, expect, bitIndex) {
    ASSERT(index, expect, WScript.CheckCustomBit(x, bitIndex));
}

function SomeObject ()
{
    this.idString = "SomeObject";
}

FLIP (0, SomeObject, false, 1);
CHECK(1, SomeObject, true, 1);

var someObjectNewInstance = new SomeObject();
CHECK(2, someObjectNewInstance, false, 1);
CHECK(3, SomeObject, true, 1);
CHECK(4, someObjectNewInstance, false, 1);
FLIP (5, SomeObject, true, 1);
CHECK(6, SomeObject, false, 1);
CHECK(7, someObjectNewInstance, false, 1);
FLIP (8, SomeObject, false, 31);
CHECK(9, SomeObject, true, 31);

var obj = new SomeObject();
// [ 10 -> 42 )
for (var i = 0; i < 32; i++) {
    CHECK(i + 10, obj, false, i);
}

// [ 42 -> 74 )
for (var i = 0; i < 32; i++) {
    FLIP(i + 42, obj, false, i);
}

// [ 74 -> 106 )
for (var i = 0; i < 32; i++) {
    CHECK(i + 74, obj, true, i);
}

// [ 106 -> 138 )
for (var i = 0; i < 32; i++) {
    FLIP(i + 106, obj, true /*unset*/, i);
}

// [ 138 -> 170 )
for (var i = 0; i < 32; i++) {
    CHECK(i + 138, obj, false, i);
}

print("PASS");
