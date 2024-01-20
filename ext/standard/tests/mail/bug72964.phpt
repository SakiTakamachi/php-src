--TEST--
Bug #72964 (White space not unfolded for CC/Bcc headers)
--EXTENSIONS--
curl
--SKIPIF--
<?php
if (getenv("SKIP_SLOW_TESTS")) die('skip slow test');
if (PHP_OS_FAMILY !== 'Windows') die('skip Windows only test');
?>
--INI--
SMTP=localhost
smtp_port=1025
sendmail_from=from@example.com
--FILE--
<?php

$to = 'bug72964_to@example.com';
$from = ini_get('sendmail_from');
$subject = bin2hex(random_bytes(16));
$message = 'hello';
$headers = "From: {$from}\r\n"
    . "Cc: bug72964_cc_1@example.com,\r\n\tbug72964_cc_2@example.com\r\n"
    . "Bcc: bug72964_bcc_1@example.com,\r\n bug72964_bcc_2@example.com\r\n";

$res = mail($to, $subject, $message, $headers);

if ($res !== true) {
    exit("Unable to send the email.\n");
} else {
    echo "Sent the email.\n";
}

$c = curl_init();
curl_setopt($c, CURLOPT_URL, 'http://localhost:8025/api/v2/messages');
curl_setopt($c, CURLOPT_RETURNTRANSFER, true);
$res = curl_exec($c);
curl_close($c);

var_dump(json_decode($res, true));
?>
--EXPECT--
Message sent OK
TEST PASSED: Message sent and deleted OK
TEST PASSED: Message sent and deleted OK
TEST PASSED: Message sent and deleted OK
TEST PASSED: Message sent and deleted OK
