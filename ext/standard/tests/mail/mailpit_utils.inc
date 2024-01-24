<?php

define('IMAP_HOST', 'localhost');
define('IMAP_PORT', 143);
define('IMAP_ADMIN_USER', 'webmaster@example.com');
define('IMAP_PASSWORD', 'p4ssw0rd');
define('IMAP_USERS', [
    'webmaster@example.com',
    'info@example.com',
    'admin@example.com',
    'foo@example.com',
]);

function getMailBox(string $emailAddress)
{
    if (($mailBox = fsockopen(IMAP_HOST, IMAP_PORT)) === false) {
        die('cannot open imap socket');
    }

    $password = IMAP_PASSWORD;
    fputs($mailBox, "? login {$emailAddress} {$password}\r\n");
    while ($outputLine = fgets($mailBox)) {
        if (preg_match('/(NO|failed|Failure)/i', $outputLine)) {
            fclose($mailBox);
            die('login failed');
        }
    }

    return $mailBox;
}

function getEmailUidsBySubject($mailBox, string $subject): array
{
    fputs($mailBox, "? SELECT \"INBOX\"\r\n");
    fread($mailBox, 1024);

    fputs($mailBox, "? UID SEARCH SUBJECT \"{$subject}\"\r\n");
    $res = trim(fgets($mailBox) ?: '');
    var_dump($res);
    fread($mailBox, 1024);

    preg_match('/SEARCH ([0-9 ]+)/i', $res, $matches);
    return explode(' ', $matches[1] ?? '');
}

function getEmailsBySubject($mailBox, string $subject): array
{
    fputs($mailBox, "? SELECT \"INBOX\"\r\n");
    fread($mailBox, 1024);

    fputs($mailBox, "? UID SEARCH SUBJECT \"{$subject}\"\r\n");
    $res = trim(fgets($mailBox) ?: '');
    fread($mailBox, 1024);

    preg_match('/SEARCH ([0-9 ]+)/i', $res, $matches);
    $uids = explode(' ', $matches[1] ?? '');

    $mails = [];
    foreach ($uids as $uid) {
        if (!$uid) {
            continue;
        }
        $mails[] = getHeaders($mailBox, $uid) + ['Body' => getBody($mailBox, $uid)];
    }
    return $mails;
}

function getHeaders($mailBox, $uid): array
{
    $key = null;
    $headers = [];
    fputs($mailBox, "? UID FETCH {$uid} RFC822.HEADER\r\n");
    while ($outputLine = fgets($mailBox)) {
        $outputLine = trim($outputLine);
        if (!$outputLine) {
            continue;
        }
        $items = explode(':', $outputLine);
        if (count($items) > 1) {
            $key = trim($items[0]);
            $val = trim($items[1]);
            $headers[$key] = [];
        } else {
            $val = trim($items[0]);
            if (!$key || $val === '? OK UID completed' || $val === ')') {
                continue;
            }
        }

        $vals = explode(',', $val);
        foreach ($vals as $val) {
            $headers[$key][] = trim($val);
        }
    }
    return $headers;
}

function getBody($mailBox, $uid): string
{
    $body = '';
    fputs($mailBox, "? UID FETCH {$uid} RFC822.TEXT\r\n");
    while ($outputLine = fgets($mailBox)) {
        $body .= $outputLine;
    }
    return str_replace(")\r\n? OK UID completed", '', $body);
}

function mailCheckResponse($res, string $from, string $to, string $subject, string $message): bool
{
    var_dump($res);
    return $res &&
        ($res['From'][0] ?? null) === $from &&
        ($res['To'][0] ?? null) === $to &&
        ($res['Subject'][0] ?? null) === $subject &&
        trim($res['Body'] ?? '') === trim($message);
}

function deleteEmailsBySubject(array $emailAddresses, string $subject)
{
    foreach ($emailAddresses as $emailAddress) {
        $mailBox = getMailBox($emailAddress);
        $uids = getEmailUidsBySubject($mailBox, $subject);
        foreach ($uids as $uid) {
            if (!$uid) {
                continue;
            }
            fputs($mailBox, "? UID STORE {$uid} +FLAGS (\\Deleted)\r\n");
            fread($mailBox, 1024);
        }
        fputs($mailBox, "? EXPUNGE\r\n");
        fread($mailBox, 1024);
        fclose($mailBox);
    }
}
?>
