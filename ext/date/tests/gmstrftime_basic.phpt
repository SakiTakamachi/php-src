--TEST--
Test gmstrftime() function : basic functionality
--FILE--
<?php
echo "*** Testing gmstrftime() : basic functionality ***\n";

// Initialise all required variables
$format = '%b %d %Y %H:%M:%S';
$timestamp = gmmktime(8, 8, 8, 8, 8, 2008);

// Calling gmstrftime() with all possible arguments
var_dump( gmstrftime($format, $timestamp) );

// Calling gmstrftime() with mandatory arguments
var_dump( gmstrftime($format) );

?>
--EXPECTF--
*** Testing gmstrftime() : basic functionality ***

Deprecated: Function gmstrftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(20) "Aug 08 2008 08:08:08"

Deprecated: Function gmstrftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(%d) "%s %d %d %d:%d:%d"
