<?php

class MailConnecter
{
    private $fp = null;
    private static $instance;
    private static $init = false;

    private const HOST = 'localhost';
    private const PORT = 143;
    private const TIMEOUT = 10;

    private function __construct()
    {
        if (($this->fp = @fsockopen(self::HOST, self::PORT)) === false) {
            die('cannot open imap socket');
        }
    }

    public static function getConnection(): self
    {
        if (!static::$init) {
            static::$instance = new self();
        }

        return static::$instance;
    }

    public function disconnect(): void
    {
        fclose($this->fp);
    }

    public function fail(string $message): void
    {
        $this->disconnect();
        die($message);
    }

    public function send(string $tag, string $command): void
    {
        fputs($this->fp, "{$tag} {$command}\r\n");
    }

    public function isSuccess(string $tag): bool
    {
        $start = time();
        while (!feof($this->fp)) {
            $line = fgets($this->fp);
            if (!$line) {
                return false;
            }
            if (str_contains($line, $tag)) {
                if (preg_match('/(NO|BAD|failed|Failure)/i', $line)) {
                    return false;
                }
                return true;
            }
            if (time() - $start > self::TIMEOUT) {
                $this->fail("{$tag} timeout");
            }
        }
        return false;
    }

    public function getResponse(string $tag, bool $returnArray = false): string|array
    {
        $start = time();
        $output = $returnArray ? [] : '';
        while (!feof($this->fp)) {
            $line = fgets($this->fp);
            if (!$line) {
                $this->fail("{$tag} failed");
            }
            if ($returnArray) {
                $output[] = $line;
            } else {
                $output .= $line;
            }
            if (str_contains($line, $tag)) {
                if (preg_match('/(NO|BAD|failed|Failure)/i', $line)) {
                    $this->fail("{$tag} failed");
                }
                return $output;
            }
            if (time() - $start > self::TIMEOUT) {
                $this->fail("{$tag} timeout");
            }
        }
        $this->fail("{$tag} failed");
    }
}

class MailBox
{
    private MailConnecter $mailConnecter;
    private const PASSWORD = 'p4ssw0rd';
    public const USERS = [
        'webmaster@example.com',
        'info@example.com',
        'admin@example.com',
        'foo@example.com',
    ];

    private const LOGIN = 'A00001';
    private const LOGOUT = 'A00002';
    private const SELECT_MAILBOX = 'A00003';
    private const SEARCH = 'A00004';
    private const FETCH_HEADERS = 'A00005';
    private const FETCH_BODY = 'A00006';

    private function __construct()
    {
        $this->mailConnecter = MailConnecter::getConnection();
    }

    public static function login(string $user): self
    {
        $self = new self();
        $self->mailConnecter->send(self::LOGIN, 'LOGIN '.$user.' '.self::PASSWORD);
        if (!$self->mailConnecter->isSuccess(self::LOGIN)) {
            $self->mailConnecter->fail('login failed');
        }
        return $self;
    }

    public function logout(): void
    {
        $this->mailConnecter->send(self::LOGOUT, 'LOGOUT');
        if (!$this->mailConnecter->isSuccess(self::LOGOUT)) {
            $this->mailConnecter->fail('logout failed');
        }
    }

    public function getMailsBySubject(string $subject): array
    {
        $this->mailConnecter->send(self::SELECT_MAILBOX, 'SELECT "INBOX"');
        if (!$this->mailConnecter->isSuccess(self::SELECT_MAILBOX)) {
            $this->mailConnecter->fail('select mailbox failed');
        }

        $this->mailConnecter->send(self::SEARCH, "UID SEARCH SUBJECT \"{$subject}\"");
        $res = $this->mailConnecter->getResponse(self::SEARCH);
        preg_match('/SEARCH ([0-9 ]+)/is', $res, $matches);
        $uids = explode(' ', $matches[1] ?? '');

        $mails = [];
        foreach ($uids as $uid) {
            if (!$uid) {
                continue;
            }
            $mails[] = $this->getHeaders($uid) + ['Body' => $this->getBody($uid)];
        }
        return $mails;
    }

    private function getHeaders(int $uid): array
    {
        $this->mailConnecter->send(self::FETCH_HEADERS, "UID FETCH {$uid} (BODY[HEADER.FIELDS (SUBJECT FROM)])");
        $res = $this->mailConnecter->getResponse(self::FETCH_HEADERS, true);

        $key = null;
        foreach ($res as $line) {
            $line = trim($line);
            if (!$line) {
                continue;
            }
            $items = explode(':', $line);
            if (count($items) > 1) {
                $key = trim($items[0]);
                $val = trim($items[1]);
                $headers[$key] = [];
            } else {
                $val = trim($items[0]);
                if (!$key || $val === self::FETCH_HEADERS.' OK UID completed' || $val === ')') {
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

    private function getBody(int $uid): string
    {
        $this->mailConnecter->send(self::FETCH_BODY, "UID FETCH {$uid} (BODY[TEXT])");
        return $this->mailConnecter->getResponse(self::FETCH_BODY);
    }
}
