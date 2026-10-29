#!/usr/bin/env python3
"""Synthetic loopback-only mailio protocol/TLS regression tests (no relay)."""
import base64
import email
import email.policy
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

PROBE = sys.argv[1]
MESSAGE = (b"From: =?UTF-8?B?5rWL6K+V?= <author@fixture.invalid>\r\n"
           b"To: to@fixture.invalid\r\nSubject: =?UTF-8?B?5Lit5paH5rWL6K+V?=\r\n"
           b"Message-ID: <synthetic@fixture.invalid>\r\n"
           b"Date: Tue, 20 Oct 2026 12:30:00 +0000\r\n"
           b"MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=fixture\r\n\r\n"
           b"--fixture\r\nContent-Type: text/html; charset=UTF-8\r\n"
           b"Content-Transfer-Encoding: base64\r\n\r\n" +
           base64.b64encode('<html><head><style>bad-style</style></head><body><p>你好 HTML</p><script>bad-script</script><img src="file:///fixture-must-not-read"><img src="https://fixture.invalid/pixel"></body></html>'.encode()) +
           b"\r\n--fixture\r\nContent-Type: application/octet-stream\r\n"
           b"Content-Disposition: attachment; filename=\"../../evil.txt\"\r\n"
           b"Content-Transfer-Encoding: base64\r\n\r\n" + base64.b64encode(b"fixture\x00attachment") +
           b"\r\n--fixture--\r\n")
HEADERS = MESSAGE.split(b"\r\n\r\n", 1)[0] + b"\r\n\r\n"

class Server:
    def __init__(self, protocol, context, starttls=False, scenario="normal"):
        self.protocol, self.context, self.starttls, self.scenario = protocol, context, starttls, scenario
        self.commands, self.envelope, self.data, self.errors = [], [], b"", []
        self.socket = socket.socket()
        self.socket.bind(("127.0.0.1", 0))
        self.socket.listen(1)
        self.socket.settimeout(25)
        self.port = self.socket.getsockname()[1]
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        try:
            conn, _ = self.socket.accept()
            conn.settimeout(20)
            if self.scenario == "stall":
                time.sleep(17)
                conn.close()
                return
            with conn:
                if not self.starttls:
                    conn = self.context.wrap_socket(conn, server_side=True)
                self.conn = conn
                if self.protocol == "imap": self.imap()
                else: self.smtp()
        except (ssl.SSLError, ConnectionResetError, BrokenPipeError):
            pass  # Expected for TLS-negative tests and closed sessions.
        except Exception as exc:
            self.errors.append(repr(exc))
        finally:
            self.socket.close()

    def write(self, data): self.conn.sendall(data)
    def line(self):
        data = bytearray()
        while not data.endswith(b"\n"):
            value = self.conn.recv(1)
            if not value: return b""
            data.extend(value)
            if len(data) > 40 * 1024 * 1024: raise AssertionError("oversized fixture request")
        return bytes(data).rstrip(b"\r\n")

    def upgrade(self): self.conn = self.context.wrap_socket(self.conn, server_side=True)

    def imap(self):
        self.write(b"* OK fixture IMAP ready\r\n")
        while line := self.line():
            self.commands.append(line.decode(errors="replace"))
            tag, cmd = line.split(b" ", 1)
            upper = cmd.upper()
            if upper == b"STARTTLS":
                if self.scenario == "refuse-starttls": self.write(tag + b" NO unavailable\r\n"); continue
                self.write(tag + b" OK begin TLS\r\n"); self.upgrade(); continue
            if upper.startswith(b"LOGIN "): self.write(tag + b" OK logged in\r\n")
            elif upper.startswith(b"EXAMINE "):
                self.write(b"* 2 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 42] identity\r\n* OK [UIDNEXT 103] next\r\n" + tag + b" OK [READ-ONLY] examined\r\n")
            elif upper.startswith(b"UID SEARCH "):
                assert upper == b"UID SEARCH 2:2", upper
                self.write(b"* SEARCH 102\r\n" + tag + b" OK search done\r\n")
            elif upper.startswith(b"UID FETCH "):
                assert b"BODY.PEEK[" in upper, upper
                header = b"BODY.PEEK[HEADER]" in upper
                body = HEADERS if header else MESSAGE
                section = b"BODY[HEADER]" if header else b"BODY[]"
                if self.scenario == "oversize":
                    self.write(b"* 2 FETCH (UID 102 " + section + b" {999999999}\r\n")
                else:
                    # UID deliberately follows BODY/FLAGS: legal order exposed an upstream identity bug.
                    self.write(b"* 2 FETCH (FLAGS (\\Flagged) " + section + b" {" + str(len(body)).encode() + b"}\r\n" + body + b" UID 102)\r\n" + tag + b" OK fetched\r\n")
            elif upper == b'LIST "" ""':
                self.write(b'* LIST (\\Noselect) "/" ""\r\n' + tag + b" OK delimiter\r\n")
            elif upper.startswith(b"LIST "):
                self.write(b'* LIST (\\HasNoChildren) "/" "INBOX"\r\n* LIST (\\Noselect) "/" "Archive"\r\n* LIST () "/" "Archive/&bUuL1Q-"\r\n' + tag + b" OK listed\r\n")
            elif upper.startswith(b"STATUS "):
                self.write(b'* STATUS "INBOX" (MESSAGES 2 RECENT 0)\r\n' + tag + b" OK status\r\n")
            elif upper == b"LOGOUT": self.write(b"* BYE goodbye\r\n" + tag + b" OK logout\r\n"); return
            else: raise AssertionError("unexpected IMAP command: " + repr(cmd))

    def smtp(self):
        self.write(b"220 fixture.invalid ready\r\n")
        auth_step = 0
        while line := self.line():
            self.commands.append(line.decode(errors="replace"))
            upper = line.upper()
            if auth_step == 1:
                assert base64.b64decode(line) == b"fixture-user"
                auth_step = 2; self.write(b"334 UGFzc3dvcmQ6\r\n"); continue
            if auth_step == 2:
                assert base64.b64decode(line) == b"fixture-secret"
                auth_step = 0; self.write(b"235 authenticated\r\n"); continue
            if upper.startswith(b"EHLO "): self.write(b"250-fixture.invalid\r\n250-STARTTLS\r\n250 AUTH LOGIN\r\n")
            elif upper == b"STARTTLS":
                if self.scenario == "refuse-starttls": self.write(b"454 unavailable\r\n"); continue
                self.write(b"220 begin TLS\r\n"); self.upgrade()
            elif upper == b"AUTH LOGIN": auth_step = 1; self.write(b"334 VXNlcm5hbWU6\r\n")
            elif upper.startswith(b"MAIL FROM:"): self.write(b"250 sender ok\r\n")
            elif upper.startswith(b"RCPT TO:"):
                self.envelope.append(line)
                self.write(b"550 recipient rejected\r\n" if self.scenario == "reject" else b"250 recipient ok\r\n")
            elif upper == b"DATA":
                self.write(b"354 continue\r\n")
                chunks = []
                while True:
                    data = self.line()
                    if data == b".": break
                    chunks.append(data[1:] if data.startswith(b"..") else data)
                self.data = b"\r\n".join(chunks)
                if self.scenario == "disconnect": self.conn.close(); return
                if self.scenario == "data-reject": self.write(b"554 message rejected\r\n")
                elif self.scenario == "malformed": self.write(b"not an SMTP response\r\n")
                else: self.write(b"250 queued synthetic only\r\n")
            elif upper == b"QUIT": self.write(b"221 goodbye\r\n"); return
            else: raise AssertionError("unexpected SMTP command: " + repr(line))


def command(*args): subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def context(root, name, san, days="2"):
    key, cert, csr = [root / (name + extension) for extension in (".key", ".pem", ".csr")]
    ext = root / (name + ".ext")
    ext.write_text("subjectAltName=" + san + "\nextendedKeyUsage=serverAuth\n")
    command("openssl", "req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=" + name, "-keyout", str(key), "-out", str(csr))
    validity = ["-not_before", "20190101000000Z", "-not_after", "20200101000000Z"] if days == "-1" else ["-days", days]
    command("openssl", "x509", "-req", "-in", str(csr), "-CA", str(root / "ca.pem"), "-CAkey", str(root / "ca.key"), "-CAcreateserial", *validity, "-extfile", str(ext), "-out", str(cert))
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(cert, key)
    return ctx


def invoke(protocol, ctx, ca, op, starttls=False, scenario="normal", host="localhost"):
    server = Server(protocol, ctx, starttls, scenario)
    env = os.environ | {"QT_QPA_PLATFORM": "offscreen"}
    run = subprocess.run([PROBE, op, host, str(server.port), "starttls" if starttls else "implicit", str(ca), scenario],
                         text=True, capture_output=True, timeout=23, env=env)
    assert run.returncode == 0, (run.returncode, run.stderr)
    result = json.loads(run.stdout)
    server.thread.join(3)
    assert not server.errors, server.errors
    assert "fixture-secret" not in run.stdout and "fixture-user" not in run.stdout, result
    return result, server


def main():
    passed = 0
    with tempfile.TemporaryDirectory(prefix="nkmail-loopback-") as tmp:
        root = Path(tmp)
        command("openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=NkMail Synthetic CA", "-keyout", str(root / "ca.key"), "-out", str(root / "ca.pem"))
        good = context(root, "localhost", "DNS:localhost,IP:127.0.0.1")
        wrong = context(root, "wrong.invalid", "DNS:wrong.invalid")
        expired = context(root, "expired", "DNS:localhost,IP:127.0.0.1", "-1")
        seen_sni = []
        good.set_servername_callback(lambda sock, name, ctx: seen_sni.append(name))
        ca = root / "ca.pem"
        for starttls in (False, True):
            folders, server = invoke("imap", good, ca, "folders", starttls)
            assert folders["ok"] and len(folders["folders"]) == 2, folders
            assert folders["folders"][0]["title"] == "Archive/测试", folders
            passed += 1
            page, server = invoke("imap", good, ca, "page", starttls)
            assert page["ok"] and page["uidValidity"] == "42", page
            assert page["messages"] == [{"uid": "102", "subject": "中文测试", "unread": True, "flagged": True}], page
            assert page["hasMore"] and page["nextOffset"] == 1, page
            assert any("BODY.PEEK[HEADER]" in cmd for cmd in server.commands), server.commands
            assert not any("STORE" in cmd or "SELECT " in cmd or "DELETE" in cmd for cmd in server.commands)
            passed += 1
            content, server = invoke("imap", good, ca, "content", starttls)
            assert content["ok"] and "你好 HTML" in content["text"], content
            assert "bad-script" not in content["text"] and "bad-style" not in content["text"], content
            assert content["attachments"] == [{"name": "evil.txt", "bytes": base64.b64encode(b"fixture\x00attachment").decode(), "path": ""}], content
            assert any("BODY.PEEK[]" in cmd for cmd in server.commands)
            passed += 1
            sent, server = invoke("smtp", good, ca, "send", starttls)
            assert sent["ok"] and sent["state"] == 0, sent
            assert any(b"bcc@fixture.invalid" in rcpt for rcpt in server.envelope), server.envelope
            parsed = email.message_from_bytes(server.data, policy=email.policy.default)
            assert not parsed.get("Bcc") and b"bcc@fixture.invalid" not in server.data, server.data
            assert str(parsed["Subject"]) == "中文协议测试", parsed["Subject"]
            assert "你好，世界。" in parsed.get_body(preferencelist=("plain",)).get_content()
            attachment = list(parsed.iter_attachments())[0]
            assert attachment.get_filename() == "测试.txt", attachment.get_filename()
            assert attachment.get_payload(decode=True) == b"synthetic attachment\0bytes", repr(attachment.get_payload(decode=True))
            passed += 1
            for ctx, anchor, reason in ((wrong, ca, "wrong-host"), (good, "", "untrusted"), (expired, ca, "expired")):
                for protocol, op in (("imap", "page"), ("smtp", "send")):
                    result, server = invoke(protocol, ctx, anchor, op, starttls)
                    assert (not result["ok"] if protocol == "imap" else result["state"] == 1), (reason, result)
                    assert not any("LOGIN" in cmd for cmd in server.commands), server.commands
                    passed += 1
        for scenario, state in (("reject", 1), ("data-reject", 1), ("disconnect", 2), ("malformed", 2)):
            result, server = invoke("smtp", good, ca, "send", scenario=scenario)
            assert result["state"] == state, (scenario, result)
            passed += 1
        for scenario in ("stale", "oversize"):
            result, server = invoke("imap", good, ca, "content", scenario=scenario)
            assert not result["ok"], (scenario, result)
            if scenario == "stale": assert not any("FETCH" in cmd for cmd in server.commands)
            passed += 1
        for protocol, op in (("imap", "page"), ("smtp", "send")):
            result, server = invoke(protocol, good, ca, op, True, "refuse-starttls")
            assert not result["ok"] if protocol == "imap" else result["state"] == 1
            assert not any("LOGIN" in cmd for cmd in server.commands), server.commands
            passed += 1
        result, _ = invoke("imap", good, ca, "page", host="127.0.0.1")
        assert result["ok"], result
        passed += 1
        assert "localhost" in seen_sni, seen_sni
        started = time.monotonic()
        result, _ = invoke("imap", good, ca, "page", scenario="stall")
        assert not result["ok"] and time.monotonic() - started < 21, result
        passed += 1
    print(f"PASS: {passed} synthetic loopback protocol/TLS scenarios")

if __name__ == "__main__": main()
