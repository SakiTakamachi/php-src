--TEST--
int min test
--INI--
error_reporting = E_ALL
--FILE--
<?php
function out (int $i)
{
    var_dump($i);
}

out(PHP_INT_MIN);

var_dump(intdiv(PHP_INT_MIN, 1));

?>
--EXPECT--
int(-9223372036854775808)
int(-9223372036854775808)
