--TEST--
Bug GH-14370 (PDO::PARAM_INT casts to 32bit int internally even on 64bit builds in pdo_mysql with libmysqlclient driver)
--EXTENSIONS--
pdo_mysql
--SKIPIF--
<?php
require_once __DIR__ . '/inc/mysql_pdo_test.inc';
MySQLPDOTest::skip();
?>
--FILE--
<?php
require_once __DIR__ . '/inc/mysql_pdo_test.inc';
$pdo = MySQLPDOTest::factory();

$pdo->query('CREATE TABLE test_14370 (id INTEGER NOT NULL, num BIGINT NOT NULL, PRIMARY KEY(id))');
$stmt = $pdo->prepare('insert into test_14370 (id, num) values (:id, :num)');

$num = PHP_INT_SIZE >= 8 ? 100004313234244 :  PHP_INT_MAX; // if 64-bit, $num exceeds 32 bits
$stmt->bindValue(':id', 1, PDO::PARAM_INT);
$stmt->bindValue(':num', $num, PDO::PARAM_INT);
$stmt->execute();

$stmt = $pdo->query('SELECT num FROM test_14370');
[$ret] = $stmt->fetchAll(PDO::FETCH_COLUMN);

if ($ret === $num) {
    echo "Same value.\n";
} else {
    echo "Value mismatch.\n";
    echo "expected: {$num}\n";
    echo "actual: {$ret}\n";
}

echo 'Done';
?>
--CLEAN--
<?php
require_once __DIR__ . '/inc/mysql_pdo_test.inc';
$pdo = MySQLPDOTest::factory();
$pdo->query('DROP TABLE IF EXISTS test_14370');
?>
--EXPECT--
Same value.
Done
