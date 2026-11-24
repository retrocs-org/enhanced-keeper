#!/usr/bin/env python3
"""Run the PHP website and private KeeperHub API together on loopback."""
import argparse
import os
from pathlib import Path
import secrets
import shutil
import subprocess
from relay import Relay
from relay_worker import serve_control
import threading


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state', type=Path, default=Path('.local/keeperhub'))
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--stop', action='store_true')
    args = parser.parse_args()
    os.umask(0o077)
    args.state.mkdir(parents=True, exist_ok=True, mode=0o700)
    stopfile = args.state/'stop.request'
    if args.stop:
        stopfile.touch()
        print('Local community service stop requested.')
        return
    php = shutil.which(os.environ.get('KFX_PHP', 'php'))
    if not php:
        parser.error('PHP 8.1+ CLI is required for the public website (set KFX_PHP if needed).')
    if not 1 <= args.port <= 65535:
        parser.error('Choose a port from 1 to 65535.')
    stopfile.unlink(missing_ok=True)
    keyfile = args.state/'admin.key'
    if not keyfile.exists():
        keyfile.write_text(secrets.token_urlsafe(32))
        keyfile.chmod(0o600)
    origin = 'http://127.0.0.1:'+str(args.port)
    relay = Relay()
    relay_key = args.state/'relay-control.key'
    if not relay_key.exists():
        relay_key.write_text(secrets.token_urlsafe(32));relay_key.chmod(0o600)
    server = serve_control(relay, relay_key.read_text().strip())
    web = Path(__file__).resolve().parents[2]/'public-web'
    environment = dict(os.environ, KFX_PUBLIC_ORIGIN=origin, KFX_HUB_STATE=str(args.state.resolve()), KFX_HUB_DB=str((args.state/'community.sqlite3').resolve()), KFX_DB_CONFIG='', KFX_DB_DSN='', KFX_DB_USER='', KFX_DB_PASSWORD='', KFX_HUB_ADMIN_KEY='', KFX_HUB_ADMIN_KEY_FILE=str(keyfile.resolve()), KFX_RELAY_CONTROL_KEY='', KFX_RELAY_CONTROL_KEY_FILE=str(relay_key.resolve()),
                       KFX_RELAY_CONTROL='http://127.0.0.1:'+str(server.server_port))
    process = None
    try:
        # Do not log account/device URLs or credentials through PHP's access log.
        process = subprocess.Popen([php, '-S', '127.0.0.1:'+str(args.port), '-t', str(web), str(web/'router.php')],
                                   env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        (args.state/'local-url.txt').write_text(origin)
        print('KeeperFX-E PHP website at '+origin, flush=True)
        server.timeout = .5
        while not stopfile.exists():
            if process.poll() is not None:
                raise SystemExit('PHP website stopped; check that the selected port is available.')
            server.handle_request()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        relay.close()
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == '__main__':
    main()
