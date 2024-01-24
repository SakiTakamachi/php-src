<?php

define('HOST', 'localhost');
define('IMAP_PORT', 143);
define('MAIL_ADMIN_USER', 'webmaster@example.com');
define('MAIL_PASSWORD', 'p4ssw0rd');
define('MAIL_USERS', [
    'webmaster@example.com',
    'info@example.com',
    'admin@example.com',
    'foo@example.com',
]);

function imap_connect(string $mailAddress): bool
{
    if (($imap = fsockopen(HOST, IMAP_PORT)) === false) {
        die('cannot open imap socket');
    }

    fputs($imap, "? login {$mailAddress} {MAIL_PASSWORD}\r\n");
    while ($outputLine = fgets($imap)) {
        if (preg_match('/(NO|failed|Failure)/i', $line)) {
            fclose($fp);
            die('login failed');
        }
    }

    return $imap;
}

function getEmailsBySubject($imap, string $subject)
{
    fputs($imap, "? SELECT \"INBOX\"\r\n");
    fread($imap, 1024);

    fputs($imap, "? UID SEARCH charset utf-8 subject \"{$subject}\"\r\n");
    $res = fgets($imap);
    fread($imap, 1024);

    preg_match('/SEARCH ([0-9 ].*)/i', $res, $matches);
    $uids = explode(' ', $matches[1] ?? '');

    $mails = [];
    foreach ($uids as $uid) {
        $mail = '';
        fputs($imap, "? UID FETCH {$uid} RFC822.HEADER\r\n");
        while ($outputLine = fgets($imap)) {
            $mail .= $outputLine;
        }
        $mails[] = $mail;
    }

    return $mails;
}

function searchEmailByToAddress(string $to): ?array
{
    $query = urlencode("to:{$to}");
    $res = file_get_contents("http://localhost:8025/api/v1/search?query=$query");
    $list = json_decode($res, true);

    $id = $list['messages'][0]['ID'] ?? null;
    if (!$id) {
        return null;
    }

    $res = file_get_contents("http://localhost:8025/api/v1/message/{$id}");
    return json_decode($res, true);
}

function getHeaders($res): array
{
    $headerRes = file_get_contents("http://localhost:8025/api/v1/message/{$res['ID']}/headers");
    return json_decode($headerRes, true);
}

function mailCheckResponse($res, string $from, string $to, string $subject, string $message): bool
{
    return $res &&
        ($res['From']['Address'] ?? null) === $from &&
        ($res['To'][0]['Address'] ?? null) === $to &&
        ($res['Subject'] ?? null) === $subject &&
        trim($res['Text'] ?? '') === trim($message);
}

function getCcAddresses($res): array
{
    return array_column($res['Cc'], 'Address');
}

function getBccAddresses($res): array
{
    return array_column($res['Bcc'], 'Address');
}

function deleteEmailByToAddress(string $to)
{
    $query = urlencode("to:{$to}");
    file_get_contents("http://localhost:8025/api/v1/search?query={$query}", false, stream_context_create([
        'http' => [
            'method' => 'DELETE',
        ],
    ]));
}
?>
