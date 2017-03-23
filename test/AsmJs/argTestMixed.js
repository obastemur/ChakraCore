//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function asm() {
    "use asm"
    function SubCount(a,b,c,d,e,f,g,h,j,k,l) {
        a = +a;
        b = +b;
        c = c | 0;
        d = d | 0;
        e = +e;
        f = +f;
        g = +g;
        h = +h;
        j = +j;
        k = +k;
        l = +l;

        return +(l);
    }

    function Count(a) {
        a = +a;
        return +SubCount(3.2, 1.2, 2, 5, 6.33, 4.88, 1.2, 2.6, 3.99, 1.2, 2.6);
    }

    return { Count: Count };
}

var total = 0;
var fnc = asm ();
for(var i = 0;i < 1e6; i++) {
    total += fnc.Count(1, 2);
}

if (parseInt(total) != parseInt(1e6 * 2.6)) throw new Error('Test Failed -> ' + total);

print('PASS')
