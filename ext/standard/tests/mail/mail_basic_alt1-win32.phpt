--TEST--
Test mail() function : basic functionality
--EXTENSIONS--
curl
--SKIPIF--
<?php

if( substr(PHP_OS, 0, 3) != 'WIN' ) {
   die('skip...Windows only test');
}

//require_once(__DIR__.'/mail_skipif.inc');
?>
--INI--
max_execution_time = 120
--FILE--
<?php
ini_set("SMTP", "localhost");
ini_set("smtp_port", 1025);
ini_set("sendmail_from", "user@example.com");

echo "*** Testing mail() : basic functionality ***\n";

$subject_prefix = "!**PHPT**!";

$to = "mail_basic_alt1-win32@example.com";
$subject = "$subject_prefix: Basic PHPT test for mail() function";
$message = <<<HERE
Description
bool mail ( string \$to , string \$subject , string \$message [, string \$additional_headers [, string \$additional_parameters]] )
Send an email message
HERE;

$res = mail($to, $subject, $message);

if ($res !== true) {
    exit("TEST FAILED : Unable to send test email\n");
} else {
    echo "Msg sent OK\n";
}

// Search for email message on the mail server using imap.


$found = false;

$c = curl_init();
curl_setopt($c, CURLOPT_URL, 'http://localhost:8025/api/v2/search?kind=to\&query='.$to);
curl_setopt($c, CURLOPT_CUSTOMREQUEST, "GET");
curl_setopt($c, CURLOPT_HTTPHEADER, ["Content-type: application/json"]);

$res = curl_exec($c);
curl_close($c);

var_export($response);

?>
--EXPECTF--
*** Testing mail() : basic functionality ***
Msg sent OK
Id of msg just sent is %d
.. delete it
TEST PASSED: Msgs sent and deleted OK
