sudo apt-get update
# optional
sudo apt-get install -y clangd
# must have
sudo apt-get install -y \
    build-essential \
    cmake \
    libasound2-dev \
    libx11-dev \
    libxrandr-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libxcursor-dev \
    libxinerama-dev \
    libwayland-dev \
    libxkbcommon-dev \
    xorg-dev

# Create the auth signing key once per Codespace. Never commit the private key.
if [[ -e Auth/ticket-private.pem || -e Server/auth-public.key ]]; then
    if [[ ! -f Auth/ticket-private.pem || ! -f Server/auth-public.key ]]; then
        echo "Auth keypair is incomplete; restore both key files before continuing." >&2
        exit 1
    fi
else
    (cd Auth && pnpm exec tsx Tickets/keygen.ts ticket-private.pem ../Server/auth-public.key)
fi

if [[ ! -e Auth/.env ]]; then
    printf 'PORT=3020\nAUTH_KEY_PATH=ticket-private.pem\n' > Auth/.env
    chmod 600 Auth/.env
fi

    source "/workspaces/emsdk/emsdk_env.sh"
