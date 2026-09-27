import os
import json
import urllib.request
import urllib.error

BASE_URL = "https://test.deribit.com/api/v2/public/get_instruments"
CURRENCY = "BTC"
KIND = "option"
EXPIRED = "false"
MAX_SYMBOLS = 10


OUTPUT_DIR = r"../config"  # Destination folder
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "symbols.json")


def fetch_deribit_symbols():
    url = f"{BASE_URL}?currency={CURRENCY}&kind={KIND}&expired={EXPIRED}"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req) as response:
            data = json.loads(response.read().decode())
            if "result" not in data:
                return []
            symbols = [item["instrument_name"] for item in data["result"]]
            return symbols[:MAX_SYMBOLS]
    except urllib.error.URLError as e:
        print(f"[ERROR] Network error: {e}")
        return []


def main():
    symbols = fetch_deribit_symbols()
    if not symbols:
        return

    config_data = {
        "currency": CURRENCY,
        "count": len(symbols),
        "instruments": symbols,
    }

    # Ensure the directory exists before saving (creates folder if it doesn't exist)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Save to the specified custom path
    with open(OUTPUT_FILE, "w") as f:
        json.dump(config_data, f, indent=4)

    print(f"[SUCCESS] Saved {len(symbols)} symbols to: {os.path.abspath(OUTPUT_FILE)}")


if __name__ == "__main__":
    main()