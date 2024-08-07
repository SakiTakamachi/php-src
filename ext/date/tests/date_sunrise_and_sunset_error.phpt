--TEST--
Test error condition of date_sunrise() and date_sunset()
--FILE--
<?php

try {
    date_sunrise(time(), 3);
} catch (ValueError $exception) {
    echo $exception->getMessage() . "\n";
}

try {
    date_sunset(time(), 4);
} catch (ValueError $exception) {
    echo $exception->getMessage() . "\n";
}

?>
--EXPECTF--
Deprecated: Function date_sunrise() is deprecated since 8.1, use date_sun_info() instead in %s on line %d
date_sunrise(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE

Deprecated: Function date_sunset() is deprecated since 8.1, use date_sun_info() instead in %s on line %d
date_sunset(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
