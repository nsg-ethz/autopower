import argparse

from .client import AutopowerClient
from .config import load_config, load_secrets
from .tls import create_secure_channel


def build_channel(cfg, sec):
    return create_secure_channel(
        cfg.host,
        cfg.port,
        sec.key,
        sec.cert,
        sec.ca,
    )


def main():
    p = argparse.ArgumentParser()

    p.add_argument("--config", required=True, help="Path to config.json")
    p.add_argument("--secrets", required=True, help="Path to secrets.json")

    sub = p.add_subparsers(dest="cmd", required=True)

    start = sub.add_parser("start")
    start.add_argument("--device", required=True)
    start.add_argument("--sampling", type=int, required=True)
    start.add_argument("--upload", type=int, required=True)
    start.add_argument("--pp", required=True)

    stop = sub.add_parser("stop")
    stop.add_argument("--device", required=True)

    ping = sub.add_parser("ping")
    ping.add_argument("--device", required=True)

    status = sub.add_parser("status")
    status.add_argument("--device", required=True)

    pp = sub.add_parser("pp")
    pp.add_argument("--device", required=True)

    args = p.parse_args()

    cfg = load_config(args.config)
    sec = load_secrets(args.secrets)

    channel = build_channel(cfg, sec)
    client = AutopowerClient(channel, cfg.mgmt_id, sec.mgmt_secret)

    if args.cmd == "start":
        resp = client.start(args.device, args.sampling, args.upload, args.pp)
        print(resp.msg)

    elif args.cmd == "stop":
        resp = client.stop(args.device)
        print(resp.msg)

    elif args.cmd == "ping":
        resp = client.ping(args.device)
        print(resp.msg)

    elif args.cmd == "status":
        print(client.status(args.device))

    elif args.cmd == "pp":
        print(client.pp_devices(args.device))


if __name__ == "__main__":
    main()
