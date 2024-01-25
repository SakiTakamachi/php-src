--TEST--
Bug #80706 (Headers after Bcc headers may be ignored)
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

$to = 'bug80706_to@example.com';
$from = ini_get('sendmail_from');
$bcc = 'bug80706_bcc@example.com';
$subject = 'mail_bug80706';
$message = 'hello';
$xMailer = 'bug80706_x_mailer';
$headers = "From: {$from}\r\n"
    . "Bcc: {$bcc}\r\n"
    . "X-Mailer: {$xMailer}";

$res = mail($to, $subject, $message, $headers);

if ($res !== true) {
    die("Unable to send the email.\n");
}

echo "Email sent.\n";

$res = searchEmailByToAddress($to);

if (mailCheckResponse($res, $from, $to, $subject, $message)) {
    echo "Found the email sent.\n";

    $bccAddresses = getBccAddresses($res);
    if (in_array($bcc, $bccAddresses, true)) {
        echo "bcc is set.\n";
    }

    $headers = getHeaders($res);
    if ($headers['X-Mailer'][0] === $xMailer) {
        echo "The specified x-Mailer exists.";
    }
}
foreach (['to' => $to, 'bcc' => $bcc] as $type => $mailAddress) {
    $mailBox = MailBox::login($mailAddress);
    $mail = $mailBox->getMailsBySubject($subject);

    if ($mail->isAsExpected($from, $to, $subject, $message)) {
        echo "Found the email. {$recipient} received.\n";
    }

    if (($mail->get('X-Mailer')[0] ?? null) === $xMailer) {
        echo "The specified x-Mailer exists.";
    }

    $mailBox->deleteMailsBySubject($subject);
    $mail = $mailBox->getMailsBySubject($subject);
    var_dump('after delete: '.count($mail));
}
?>
--CLEAN--
<?php
//require_once __DIR__.'/mailpit_utils.inc';
//deleteEmailByToAddress('bug72964_to@example.com');
?>
--EXPECT--
Email sent.
Found the email sent.
bcc is set.
The specified x-Mailer exists.
