//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var tests = [
    function()
    {
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
    },
    function() {
        function asm() {
            "use asm"
            function SubCount(a,b,c,d,e,f,g) {
                a = a | 0;
                b = b | 0;
                c = c | 0;
                d = d | 0;
                e = e | 0;
                f = f | 0;
                g = +g;

                return +(g);
            }

            function Count(a) {
                a = +a;
                return +SubCount(1, 2, 3, 4, 5, 6, 1.3);
            }

            return { Count: Count };
        }

        var total = 0;
        var fnc = asm ();
        for(var i = 0;i < 1e6; i++) {
            total += fnc.Count(1, 2);
        }

        if (parseInt(total) != parseInt(1e6 * 1.3)) throw new Error('Test Failed -> ' + total);
    }];

for(var i = 0; i < tests.length; i++)
    tests[i]();

print('PASS')
