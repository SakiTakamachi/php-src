<?php

// Test defaults, assumes the following:
//
// Username: 	SYSDBA
// Password:	phpfi
// Database:	<nothing>
//
// A DSN must be specified by using PDO_FIREBIRD_TEST_DSN

define('PDO_FIREBIRD_TEST_USER', getenv('PDO_FIREBIRD_TEST_USER') ?: 'SYSDBA');
define('PDO_FIREBIRD_TEST_PASS', getenv('PDO_FIREBIRD_TEST_PASS') ?: 'phpfi');
define('PDO_FIREBIRD_TEST_DSN', getenv('PDO_FIREBIRD_TEST_DSN') ?: '');

if(!PDO_FIREBIRD_TEST_DSN)
{
        die('Error: PDO_FIREBIRD_TEST_DSN must be set');
}

function getDbConnection($class = PDO::class): PDO {
    return new $class(PDO_FIREBIRD_TEST_DSN, PDO_FIREBIRD_TEST_USER, PDO_FIREBIRD_TEST_PASS);
}

function connectToDb(): Pdo\Firebird {
    return Pdo\Firebird::connect(PDO_FIREBIRD_TEST_DSN, PDO_FIREBIRD_TEST_USER, PDO_FIREBIRD_TEST_PASS);
}

?>
