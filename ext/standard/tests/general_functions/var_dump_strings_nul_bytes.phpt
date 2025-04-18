--TEST--
Test var_dump() function with strings containing nul bytes
--FILE--
<?php

function check_var_dump($variables) {
    $counter = 1;
    foreach( $variables as $variable ) {
        echo "-- Iteration $counter --\n";
        var_dump($variable);
        $counter++;
    }
}

$strings = [
  "\0",
  "abcd\x0n1234\x0005678\x0000efgh\xijkl", // strings with hexadecimal NULL
  "abcd\0efgh\0ijkl\x00mnop\x000qrst\00uvwx\0000yz", // strings with octal NULL
];

check_var_dump($strings);

?>
--EXPECT--
-- Iteration 1 --
string(1) " "
-- Iteration 2 --
string(29) "abcd n1234 05678 00efgh\xijkl"
-- Iteration 3 --
string(34) "abcd efgh ijkl mnop 0qrst uvwx 0yz"
