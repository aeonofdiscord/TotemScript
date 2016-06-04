TotemScript is a light-weight, general-purpose scripting language.
### Examples
```
print("Hello, World!");
```
#### Defining Variables
Variables are dynamically-typed, supporting the following types:
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
#### Variable Scope
```
// Variables declared outside of functions are in global scope, and can be accessed by any function.
$global = "This can be accessed by any function.";

function test()
{
    return $global + " See?";
}

// Variables declared inside of functions are in local scope, and can only be accessed by that function.
function localTest()
{
    $a = 123;

    $b = function()
    {
        return $a; // scope error
    }
}

```
#### Combining scripts
```
include "..otherDir/otherFile.totem";
// include statements must be at the top of the file
// file paths are relative to the current file's path
```
#### Functions
```
// declaring a function
// named functions can only be declared in global scope
function test($a, $b)
{
    return $a + $b;
}

$a = test(123, 456); 
$a = test(123); // arguments default to an integer value of 0 when not provided by caller

$b = @test; // functions can also be stored in variables
$a = $b(123, 456, 789); // additional arguments provided by caller are discarded

// functions can also be anonymous, which can be declared anywhere
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
#### Objects
```
// Create new garbage-collected object
$obj = {};

// Objects map strings to values
$obj["test"] = 123;
$val = $obj["test"];

// Objects can store any sort of value, but can only use strings as keys
$val["test2"] = function($x, $y)
{
    return $x * $y;
};

$val["test2"](123, 456);

// Objects are less efficient than arrays, but don't need to be manually resized
$key = "key";
$obj[$key] = [20];

for($i = 0; $i < 20; $i++)
{
    $obj[$key][$i] = $i;
}
```
#### Coroutines
```
// Coroutines are functions that pause when they return, and can be resumed later

// Coroutines are created by casting any function to a coroutine, and invoked like a regular function
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
### Feature Creep
#### Language Features
* object as string
* object as array
* array as object
* string as object
* garbage-collected lua-like userdata with destructors
* FIFO message channels, e.g. $channel = <>; $channel push $x; $channel pop $y;
* function error handlers, catch runtime errors e.g. function x($a, $b) { causeAnError(); } unless { somethingBadHappened(); }
* operator precedence reordering
* initializer lists for arrays & objects, both with and without indices/names e.g. $a = { "x":1.75, "y":1.45 }; $b = [$a, 123, "456"];
* loop scope for vars
* function arg improvements
 * default arguments for functions e.g. function call($x: 123, $y: 456);
 * specify argument name in direct function calls, e.g. $x = call($y: 123, $x: 456);
* switch statement

#### Runtime Improvements
* ref-cycle detection
* bytecode serialisation
 * check register & function addresses, function arguments
* debug information for scripts
 * line numbers
 * variable->register mappings
* breakpoints
 * lookup first instruction at given line of source code, replace with breakpoint instruction, store original
 * call user-supplied callback, then execute original instruction
* line/char/len numbers for eval, link & exec errors
* better built-in support for concurrency
 * memory
  * gc-object creation, collection & ref-cycle detection isolated to individual exec states
  * synchronise channels
 * function pointers must reference actor
* use setjmp/longjmp to break instruction execution instead of checking return value
 * allows simple subroutine threading