TotemScript is a light-weight, general-purpose scripting language.
### Examples
```
print("Hello, World!");
```
#### Defining Variables
Variables are dynamic by default, supporting the following types:
* int - 64-bit signed integers
* float - 64-bit floating point
* string - Interned, immutable strings
* function - First-class functions
* array - Garbage-collected Arrays
* coroutine - First-class, garbage collected coroutines
* type - Type objects (e.g. int, float, type etc.)
```
// variables can be declared and redeclared at any point
$var = 123;
$var = "This is now a string.";

function test()
{
    // this will reference the variable in global-scope
    $var = 456;

    // vars can also be read-only
    const $x = "This variable cannot be modified";
}
```
#### Functions
```
// declaring a function
function test($a, $b)
{
    return $a + $b;
}

$a = test(123, 456); 
$a = test(123); // arguments default to an integer value of 0 when not provided by caller

$b = @test; // functions can also be stored in variables
$a = $b(123, 456, 789); // additional arguments provided by caller are discarded

// functions can also be anonymous
$b = function($a, $b)
{
    return $a * $b;
};

$a = $b(123, 456);

$a = function($b, $c)
{
    return $b - $c;
}
(456, 123);
```
#### Strings
```
// Strings are immutable
$a = "Hello, ";
$b = $a + " World!";
print($b); // Hello, World!
$c = $a[2];
print($c); // l
```
#### Arrays
```
// Defines an Array that holds 20 values
$a = [20];

$a[0] = 1;
$a[1] = 2;

$b = $a[0] + $a[1];

// Arrays cannot be resized and must be redeclared if additional space is needed
$a[20] = 1; // runtime error

$a = "some other value"; // Arrays are automatically garbage-collected when no-longer referenced
```
#### Coroutines
```
// Coroutines are functions that pause when they return, and can be resumed later

// Coroutines are created by casting any function to a coroutine
$co = function($start, $end)
{
    for($i = $start; $i < $end; $i++)
    {
        return $i + $start + $end;
    }

    return 0;
} as coroutine;

$start = 11;
$end = 20;

// Parameters are ignored on subsequent calls
for($numLoops = 0; $val = $co($start, $end); $numLoops++)
{
    print($val);
}

// Coroutines reset once they reach the end of a function
for($numLoops = 0; $val = $co($start + 1, $end + 1); $numLoops++)
{
    print($val);
}

// Coroutines are also garbage-collected, just like Arrays - when no longer referenced, they are destroyed
$co = 123;
```
### Todo
#### Language Features
* garbage-collected string->value maps, e.g. $x = {"whatever":123, "this":@hello}; $y = $x{"this"}($x{"whatever"});
* garbage-collected lua-like userdata with destructors
 
#### Runtime Improvements
* ref-counting cycle detection
* line/char/len numbers for eval, link & exec errors
* better concurrency
 * separate scripts from runtime
 
### Feature Creep
#### Language Features
* exceptions - try, catch, finally & throw
 * both user & system-generated exceptions
* operator precedence reordering
* initializer lists for arrays & records, both with and without indices/names e.g. $a:vec2 = { $x:1.75, $y:1.45 };
* loop scope for vars
* function arg improvements
 * default arguments for functions e.g. function call($x:int = 123, $y:int = 456);
 * specify argument name in direct function calls, e.g. $x = call($y: 123, $x: 456);

#### Runtime Improvements
* pre-compute value-only expressions
* unroll determinate loops
* escape analysis for arrays
* bytecode serialisation
 * check register & function addresses, function arguments
* register-allocation improvement
 * track how many times a variable is referenced when evaluating values
 * local const vars should eval to global vars wherever possible
 * global const vars should eval straight to value register
 * piggy-back on free global registers if any are available and local scope is full
* make sequences of array / string concatenation more efficient
* debug information for scripts
 * line numbers
 * variable->register mappings
* breakpoints
 * lookup first instruction at given line of source code, replace with breakpoint instruction, store original
 * call user-supplied callback, then execute original instruction