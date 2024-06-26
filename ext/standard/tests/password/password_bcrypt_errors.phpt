--TEST--
Test error operation of password_hash() with bcrypt hashing
--FILE--
<?php
//-=-=-=-
try {
    password_hash("foo", PASSWORD_BCRYPT, array("cost" => 3));
} catch (ValueError $exception) {
    echo $exception->getMessage() . "\n";
}

try {
    var_dump(password_hash("foo", PASSWORD_BCRYPT, array("cost" => 32)));
} catch (ValueError $exception) {
    echo $exception->getMessage() . "\n";
}

try {
    var_dump(password_hash("null\0password", PASSWORD_BCRYPT));
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Invalid bcrypt cost parameter specified: 3
Invalid bcrypt cost parameter specified: 32
Bcrypt password must not contain null character
