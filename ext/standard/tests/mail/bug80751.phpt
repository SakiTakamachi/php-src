--TEST--
Bug #80751 (Comma in recipient name breaks email delivery)
--EXTENSIONS--
curl
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Windows') die('skip Windows only test');
if (getenv("SKIP_SLOW_TESTS")) die('skip slow test');
//require_once __DIR__ . '/mail_skipif.inc';
?>
--INI--
SMTP=localhost
smtp_port=1025
--FILE--
<?php

$to = ['bug80751_to_1@example.com', 'bug80751_to_2@example.com'];
$toLine = "\"<{$to[0]}>\" <{$to[1]}>";
$from = ['bug80751_from_1@example.com', 'bug80751_from_2@example.com'];
$cc = 'bug80751_cc_1@example.com';
$bcc = 'bug80751_bcc_1@example.com';
$subject = bin2hex(random_bytes(16));
$message = 'hello';
$headers = "From: \"<{$from[0]}>\" <{$from[1]}>\r\n"
    . "Cc: \"Lastname, Firstname\\\\\" <{$cc}>\r\n"
    . "Bcc: \"Firstname \\\"Ni,ck\\\" Lastname\" <{$bcc}>\r\n";

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
Sent the email
