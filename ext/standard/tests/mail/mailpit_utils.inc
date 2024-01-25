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
    $start = microtime(true);
    if (($mailBox = fsockopen(IMAP_HOST, IMAP_PORT)) === false) {
        die('cannot open imap socket');
    }

    $password = IMAP_PASSWORD;
    fputs($mailBox, "1 login {$emailAddress} {$password}\r\n");
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\1 OK/i', $outputLine)) {
            break;
        }
        if (preg_match('/(NO|failed|Failure)/i', $outputLine)) {
            fclose($mailBox);
            die('login failed');
        }
    }
    var_dump('get mail box: '.microtime(true) - $start);

    return $mailBox;
}

function getEmailUidsBySubject($mailBox, string $subject): array
{
    $start = microtime(true);
    fputs($mailBox, "2 SELECT \"ANY\"\r\n");
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/(NO|failed|Failure)/i', $outputLine)) {
            var_dump('invaid mailbox: '.$outputLine);
        }
    }

    fputs($mailBox, "3 SELECT \"INBOX\"\r\n");
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\3 OK/i', $outputLine)) {
            break;
        }
    }

    fputs($mailBox, "4 UID SEARCH SUBJECT \"{$subject}\"\r\n");
    $res = trim(fgets($mailBox) ?: '');
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\4 OK/i', $outputLine)) {
            break;
        }
    }

    preg_match('/SEARCH ([0-9 ]+)/i', $res, $matches);
    var_dump('get uids: '.microtime(true) - $start);
    return explode(' ', $matches[1] ?? '');
}

function getEmailsBySubject($mailBox, string $subject): array
{
    $start = microtime(true);
    fputs($mailBox, "5 SELECT \"INBOX\"\r\n");
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\5 OK/i', $outputLine)) {
            break;
        }
    }

    fputs($mailBox, "6 UID SEARCH SUBJECT \"{$subject}\"\r\n");
    $res = trim(fgets($mailBox) ?: '');
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\6 OK/i', $outputLine)) {
            break;
        }
    }

    preg_match('/SEARCH ([0-9 ]+)/i', $res, $matches);
    $uids = explode(' ', $matches[1] ?? '');

    $mails = [];
    foreach ($uids as $uid) {
        if (!$uid) {
            continue;
        }
        $mails[] = getHeaders($mailBox, $uid) + ['Body' => getBody($mailBox, $uid)];
    }
    var_dump('get mails: '.microtime(true) - $start);
    return $mails;
}

function getHeaders($mailBox, $uid): array
{
    $start = microtime(true);
    $key = null;
    $headers = [];
    fputs($mailBox, "7 UID FETCH {$uid} RFC822.HEADER\r\n");
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\7 OK/i', $outputLine)) {
            break;
        }
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
    var_dump('get headers: '.microtime(true) - $start);
    return $headers;
}

function getBody($mailBox, $uid): string
{
    $start = microtime(true);
    $body = '';
    fputs($mailBox, "8 UID FETCH {$uid} RFC822.TEXT\r\n");

    $first = false;
    while (!feof($mailBox)) {
        $outputLine = fgets($mailBox);
        if (preg_match('/^\8 OK/i', $outputLine)) {
            break;
        }
        /*
        if (!$first) {
            $first = true;
            continue;
        }
        */
        $body .= $outputLine;
    }
    var_dump('get body: '.microtime(true) - $start);
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
    $start = microtime(true);
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
    var_dump('delete: '.microtime(true) - $start);
}
?>
