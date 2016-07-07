### TotemScript?
* A small, accessible language built for no particular reason.
* Heavily inspired by languages like LUA, PHP and JavaScript.
* Designed to be small, simple and dumb - easily plugged into existing applications.
* Features dynamic typing, full garbage-collection, cooperative multitasking, and a silly name.

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
* array - Fixed-size Arrays
* object - String/Value Maps
* coroutine - First-class coroutines
* type - Type objects (e.g. int, float, type etc.)
* userdata - Data supplied by native C functions
* null
* true
* false
```
// variables are declared thusly:
var $var = 123;

// variables can hold values of any type
$var = "This is now a string.";

function test()
{
    // this will reference the variable in global-scope
    $var = 456;

    // variables can also be declared as read-only
    let $x = "This variable cannot be modified";

    // values can be typecast
    $var = 123 as string;

    // can check type at runtime
    if($var is string)
    {
        // var is a string!
    }

    // can also retrieve type as value
    $type = $var as type;

    // values can also be "shifted" from one location to another
    $a << $type;
}
```
#### Variable Scope
```
// Variables declared outside of functions are in global scope, and can be accessed by any function.
var $global = "This can be accessed by any function.";

function test()
{
    return $global + " See?";
}

// Variables declared inside of functions are in local scope, and can only be accessed by that function.
function localTest()
{
    var $a = 123;

    var $b = function()
    {
        return $a; // scope error
    }

    // global variable names can be overridden with the "local" keyword
    // this will prevent global vars of the same name from being overwritten
    local var $global = 123;
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
function test(var $a, var $b)
{
    return $a + $b;
}

var $a = test(123, 456); 
$a = test(123); // arguments default to null when not provided by caller

var $b = @test; // functions can also be stored in variables
$a = $b(123, 456, 789); // additional arguments provided by caller are discarded

// functions can also be anonymous, which can be declared anywhere
$b = function(var $a, var $b)
{
    return $a * $b;
};

$a = $b(123, 456);

$a = function(var $b, var $c)
{
    return $b - $c;
}
(456, 123);
```
#### Strings
```
// Strings are immutable, but can be combined to create new strings
var $a = "Hello, ";
var $b = $a + " World!";
print($b); // Hello, World!
var $c = $a[2];
print($c); // l
```
#### Arrays
```
// Defines an Array that holds 20 values
var $a = [20];

for(var $i = 0; $i < 20; $i++)
{
	$a[$i] = $i;
}

var $b = $a[0] + $a[1];

// Arrays cannot be resized and must be redeclared if additional space is needed
$a[20] = 1; // runtime error

$a = "some other value"; // Arrays are automatically garbage-collected when no-longer referenced
```
#### Objects
```
// Create new garbage-collected object
var $obj = {};

// Objects map values to names
$obj.test = 123;
var $val = $obj.test;

$obj["test"] = 456; // objects can use either bracket-notation or dot-notation

// Objects can store any sort of value, but can only use strings as keys
$obj["test2"] = function($x, $y)
{
    return $x * $y;
};

$val["test2"](123, 456);

// Objects are less efficient than arrays, but can hold any amount of values 
var $key = "key";
$obj[$key] = [20];

for(var $i = 0; $i < 20; $i++)
{
    $obj[$key][$i] = $i;
}

// values can be removed by shifting them out
$key << $obj[$key];
```
#### Coroutines
```
// Coroutines are functions that pause when they return, and can be resumed later

// Coroutines are created by casting any function to a coroutine, and invoked like a regular function
var $co = function(var $start, var $end)
{
    for(var $i = $start; $i < $end; $i++)
    {
        return $i + $start + $end;
    }

    return 0;
} as coroutine;

var $start = 11;
var $end = 20;

// Parameters are ignored on subsequent calls
for(var $numLoops = 0; var $val = $co($start, $end); $numLoops++)
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
* TryExec
* Default Argument Values & skipping arguments
* Instances - load a script as a callable value, e.g. $a = <test.totem>; $a(); $a.callNamedFunction();
* Compile-time type hinting, e.g. $a:int = 0; type number = int|float; $b:number = 0.0; $c = $b is number; function test:number() { return 123; }
* Complex custom classes that eval to arrays at runtime, e.g. type Vec2 = {public $x:0.0, public $y:0.0, public lengthSquared:float() { return ($x * $x) + ($y * $y;) }}; $a:Vec2 = {$x:123.0, $y:456.0}; $b:float = $a.lengthSquared();

### Runtime Improvements
* Function Arguments should be laid out in register stack rather than as instructions
* Separate "current instruction" from loop calls