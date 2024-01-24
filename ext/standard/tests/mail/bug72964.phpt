--TEST--
Bug #72964 (White space not unfolded for CC/Bcc headers)
--SKIPIF--
<?php
if (getenv("SKIP_SLOW_TESTS")) die('skip slow test');
require_once __DIR__.'/mail_windows_skipif.inc';
?>
--INI--
SMTP=localhost
smtp_port=25
sendmail_from=from@example.com
--FILE--
<?php

require_once __DIR__.'/mailpit_utils.inc';

$users = IMAP_USERS;

$to = $users[0];
$from = ini_get('sendmail_from');
$cc = [$users[0], $users[1]];
$bcc = [$users[2], $users[3]];
$subject = 'mail_bug72964';
$message = 'hello';
$headers = "From: {$from}\r\n"
    . "Cc: {$cc[0]},\r\n\t{$cc[1]}\r\n"
    . "Bcc: {$bcc[0]},\r\n {$bcc[1]}\r\n";

$res = mail($to, $subject, $message, $headers);

if ($res !== true) {
    die("Unable to send the email.\n");
}

echo "Email sent.\n";

foreach ([
    'to' => [$to],
    'cc' => $cc,
    'bcc' => $bcc,
] as $type => $emailAddresses) {
    foreach ($emailAddresses as $emailAddress) {
        $mailBox = getMailBox($emailAddress);
        $res = getEmailsBySubject($mailBox, $subject);
        fclose($mailBox);

        if (mailCheckResponse($res, $from, $to, $subject, $message)) {
            echo "Found the email. {$type} received.\n";
        }
    }
}
?>
--CLEAN--
<?php
require_once __DIR__.'/mailpit_utils.inc';
foreach (IMAP_USERS as $emailAddress) {
    deleteEmailsBySubject($emailAddress, 'mail_bug72964');
}
?>
--EXPECT--
Email sent.
Found the email. to received.
Found the email. cc received.
Found the email. cc received.
Found the email. bcc received.
Found the email. bcc received.
