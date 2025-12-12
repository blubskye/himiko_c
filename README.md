<p align="center">
  <img src="himiko.png" alt="Himiko" width="400"/>
</p>

<h1 align="center">💉 Himiko Discord Bot (C Edition) 💉</h1>

<p align="center">
  <em>"I just wanna love you, wanna be loved~"</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C-11-A8B9CC?style=for-the-badge&logo=c&logoColor=white" alt="C"/>
  <img src="https://img.shields.io/badge/SQLite-003B57?style=for-the-badge&logo=sqlite&logoColor=white" alt="SQLite"/>
  <img src="https://img.shields.io/badge/Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white" alt="Discord"/>
  <img src="https://img.shields.io/badge/License-AGPL--3.0-red?style=for-the-badge" alt="License"/>
</p>

---

## 🩸 About Himiko

A feature-rich Discord bot written in C using the [Concord](https://github.com/Cogmasters/concord) library, with SQLite storage. This is a port of the original Go version, offering the same features with improved performance and lower memory footprint.

> *"Let me help you... I promise I won't bite~ Much."*

---

## ✨ Features

### 🔪 Administration
- **Moderation:** Kick, ban, unban, softban, hackban
- **Timeout:** Timeout and remove timeout
- **Messages:** Purge messages (with filters)
- **Channel Control:** Slowmode, lock/unlock channels
- **Warning System:** Track troublemakers~
- **View Bans:** See who's been naughty

### 🎀 XP & Leveling System
- **Track Activity:** Users earn XP by chatting
- **Leaderboards:** See who's the most active!
- **Level Roles:** Auto-assign roles at level milestones
- **Voice XP:** Earn XP in voice channels too~
- **Admin Controls:** Set levels, add XP, mass XP operations

### 🛡️ Auto-Moderation
- **Regex Filters:** Custom pattern matching with actions
- **Spam Filter:** Limit mentions, links, and emojis
- **Per-Channel Config:** Disable logging for specific channels

### 🚨 Anti-Raid Protection
- **Raid Detection:** Automatic detection of mass joins
- **Auto-Silence Modes:** Log, alert, raid-only, or all-joins silencing
- **Server Lockdown:** Automatic verification level raise during raids
- **Silence/Unsilence:** Manual and timed user silencing
- **Ban Raid:** Bulk ban detected raid users

### 🔥 Advanced Anti-Spam (Pressure System)
- **Pressure-Based Detection:** Accumulates spam pressure per user
- **Configurable Penalties:** Images, links, pings, length, repeats
- **Decay System:** Pressure naturally decreases over time
- **Actions:** Delete, warn, silence, kick, or ban spammers

### 📝 Logging System
- **Message Logs:** Deleted/edited messages
- **Voice Logs:** Join/leave events
- **User Changes:** Nicknames, avatars
- **Configurable:** Enable/disable each log type

### 🧹 Auto-Clean System
- **Channel Cleaning:** Automatically clean channels on schedule
- **Warning Messages:** Warn users before cleaning
- **Preserve Options:** Keep images if desired

### 🎲 Fun Commands
- 8-ball, dice rolls, coinflip
- Rock Paper Scissors, jokes, rate things
- Social interactions (hug, slap, pat, kiss)
- Would you rather, truth or dare

### 📝 Text Transformations
- ASCII art, Zalgo, reverse, upside down
- Morse code, Vaporwave, OwO, mock, Leet
- Encode/decode (base64, hex, binary, rot13)

### 🖼️ Images
- Random animal images (cat, dog, fox, bird, etc.)
- User avatar and banner, server icon
- Animal facts, random memes

### 🔧 Utility & Information
- Ping, snipe, AFK, reminders, polls
- User/Server/Channel/Role info
- Weather, Urban Dictionary, Wikipedia lookups

### 🎫 Ticket System
- Submit tickets to staff channels
- Clean interface with formatted embeds

### 💬 Mention Responses
- Custom trigger responses when bot is mentioned
- Optional image support

### 🎵 Music System
- **Voice Playback:** Play audio in voice channels
- **YouTube Support:** Search and play from YouTube via yt-dlp
- **Queue Management:** Add, remove, shuffle, and clear queue
- **Playback Controls:** Play, pause, resume, skip, stop, seek
- **Volume Control:** Adjustable volume (0-200%)
- **Loop Modes:** Loop track or entire queue

### 🤖 AI Integration
- Ask AI questions (requires OpenAI-compatible API)

### 🔄 Auto-Update System
- Automatic update checking from GitHub
- Manual update commands for bot owners
- Download and apply updates in-place

### 🐛 Debug Mode
- Full stack traces for errors
- Memory statistics
- Caller info logging

---

## 💉 Setup

### Prerequisites

- **C Compiler:** GCC or Clang with C11 support
- **CMake:** Version 3.15+
- **Libraries:**
  - [Concord](https://github.com/Cogmasters/concord) - Discord library (with voice support for music)
  - libcurl 7.56.1+ - HTTP client
  - SQLite3 - Database
  - json-c - JSON parsing
  - libopus - Audio encoding (optional, for music)
  - libsodium - Voice encryption (optional, for music)
  - FFmpeg - Audio decoding (runtime, for music)
  - yt-dlp - YouTube URL resolution (runtime, for music)

### Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| **Linux** | ✅ Full | Primary platform |
| **Windows** | ✅ Cygwin | Requires Cygwin environment |
| **macOS** | ✅ Full | 10.9+ |
| **FreeBSD** | ✅ Full | 12+ |

---

### Linux Setup

**Fedora/RHEL:**
```bash
sudo dnf install gcc cmake curl-devel sqlite-devel json-c-devel
```

**Ubuntu/Debian:**
```bash
sudo apt install gcc cmake libcurl4-openssl-dev libsqlite3-dev libjson-c-dev
```

**Install Concord:**
```bash
git clone https://github.com/Cogmasters/concord.git
cd concord
make
sudo make install
```

---

### Windows Setup (Cygwin)

**Note:** Native MinGW64/MSYS2 are NOT supported by Concord. You must use Cygwin.

1. **Install Cygwin** from https://cygwin.com/install.html

2. **During Cygwin installation**, select these packages:
   - `gcc-core`
   - `make`
   - `cmake`
   - `git`
   - `libcurl-devel`
   - `libsqlite3-devel`
   - `libjson-c-devel`

3. **Open Cygwin terminal** and install Concord:
   ```bash
   git clone https://github.com/Cogmasters/concord.git
   cd concord
   make
   make install
   ```

4. **Build Himiko** (see below)

**Note:** When compiling on Cygwin, you may need to add:
```bash
export CFLAGS="-L/usr/local/lib -I/usr/local/include"
```

---

### Build Himiko

```bash
git clone https://github.com/blubskye/himiko-c.git
cd himiko-c
make
```

The binary will be at `build/bin/himiko`.

### 3. Configure

Copy `config.example.json` to `config.json` and fill in your details:

```json
{
  "token": "YOUR_BOT_TOKEN_HERE",
  "prefix": "/",
  "database_path": "himiko.db",
  "owner_id": "YOUR_DISCORD_USER_ID",
  "owner_ids": ["YOUR_DISCORD_USER_ID"],
  "apis": {
    "weather_api_key": "",
    "openai_api_key": "",
    "openai_base_url": "https://api.openai.com/v1",
    "openai_model": "gpt-3.5-turbo"
  },
  "features": {
    "dm_logging": false,
    "command_history": true,
    "auto_update": true,
    "auto_update_apply": false,
    "debug_mode": false
  }
}
```

### 4. Run

```bash
./build/bin/himiko
```

Or use the Makefile:
```bash
make run
```

---

## 🎀 Getting a Bot Token

1. Go to the [Discord Developer Portal](https://discord.com/developers/applications)
2. Create a new application
3. Go to the "Bot" section
4. Click "Add Bot"
5. Copy the token
6. Enable **all** Privileged Gateway Intents:
   - Presence Intent
   - Server Members Intent
   - Message Content Intent

---

## 💕 Inviting Himiko

Use the OAuth2 URL generator in the Developer Portal, or:

```
https://discord.com/api/oauth2/authorize?client_id=YOUR_CLIENT_ID&permissions=8&scope=bot%20applications.commands
```

---

## 📋 Build Targets

| Target | Description |
|--------|-------------|
| `make` | Build Linux binary |
| `make clean` | Remove build directory |
| `make dist` | Create release archives |
| `make run` | Build and run |
| `make help` | Show all targets |

---

## 🚀 Advanced Build Options

### Optimized Build (O3 + LTO + Graphite)

For maximum performance:

```bash
mkdir build-optimized && cd build-optimized
cmake -DENABLE_OPTIMIZED=ON -DENABLE_LTO=ON ..
make
```

### Profile-Guided Optimization (PGO)

For best performance tuned to your usage:

```bash
# Step 1: Build with instrumentation
mkdir build-pgo && cd build-pgo
cmake -DENABLE_OPTIMIZED=ON -DENABLE_LTO=ON -DENABLE_PGO_GEN=ON ..
make

# Step 2: Run the bot and use it normally
./bin/himiko -c config.json
# Use various commands, then Ctrl+C to stop

# Step 3: Rebuild with profile data
cmake -DENABLE_PGO_USE=ON ..
make
```

### Sanitizer Builds (for debugging)

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..

# ThreadSanitizer (for race condition detection)
cmake -DENABLE_TSAN=ON ..

# MemorySanitizer (for uninitialized memory detection)
cmake -DENABLE_MSAN=ON ..
```

### AFL++ Fuzzing

For security testing:

```bash
mkdir build-fuzz && cd build-fuzz
CC=/path/to/afl-gcc-fast cmake -DENABLE_FUZZING=ON ..
make

# Run fuzzer
afl-fuzz -i ../fuzz/corpus/duration -o /tmp/fuzz_out -- ./fuzz/fuzz_duration
```

Available fuzz targets: `fuzz_config`, `fuzz_duration`, `fuzz_math`, `fuzz_mentions`, `fuzz_text`

---

## 🩸 Commands List

| Category | Commands |
|----------|----------|
| **Admin** | kick, ban, unban, softban, hackban, timeout, untimeout, purge, slowmode, lock, unlock, warn, warnings, clearwarnings, bans |
| **XP** | xp, rank, leaderboard, setlevel, setxp, addxp, massaddxp, addrank, removerank, listranks |
| **Anti-Raid** | antiraid, silence, unsilence, getraid, banraid, lockdown |
| **Anti-Spam** | antispam (enable/disable/set/status) |
| **Spam Filter** | spamfilter (enable/disable/set/status) |
| **Logging** | setlogchannel, togglelogging, logconfig, logstatus, disablechannellog, enablechannellog |
| **AutoClean** | autoclean (add/remove/list), setcleanmessage, setcleanimage |
| **Fun** | 8ball, dice, coinflip, rps, random, joke, rate, ship, hug, slap, pat, kiss, wyr, tod, choose |
| **Text** | ascii, zalgo, reverse, upsidedown, morse, vaporwave, owo, mock, leet, encode, decode |
| **Images** | cat, dog, fox, bird, avatar, banner, servericon, catfact, dogfact, meme |
| **Utility** | ping, snipe, afk, remind, poll, embed, uptime, math |
| **Info** | userinfo, serverinfo, channelinfo, roleinfo, botinfo, membercount |
| **Lookup** | weather, urban, wiki, crypto, github |
| **Random** | advice, quote, fact, trivia, dadjoke, password |
| **Tools** | tinyurl, qrcode, timestamp, snowflake, permissions |
| **Ticket** | ticket, setticket, disableticket, ticketstatus |
| **Mentions** | mention (add/remove/list) |
| **Settings** | setprefix, setmodlog, setwelcome, settings |
| **Music** | play, skip, stop, pause, resume, queue, nowplaying, volume, join, leave, shuffle, loop, remove, clear, seek |
| **AI** | ask |
| **Update** | update (check/apply/version) |

---

## 🔄 Database Compatibility

The C edition uses the **same SQLite database schema** as the Go version. You can:
- Use an existing `himiko.db` from the Go version
- Switch between Go and C versions seamlessly
- Run both versions against the same database (not simultaneously)

---

## 💉 Source Code

This bot is licensed under **AGPL-3.0**. Source code available at:

**https://github.com/blubskye/himiko-c**

*"I'll always be transparent with you~ That's true love, right?"*

---

## 🩸 License

```
Himiko Discord Bot (C Edition)
Copyright (C) 2025 Himiko Contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.
```

---

<p align="center">
  <em>Made with 💉 and obsessive love</em>
</p>

<p align="center">
  <img src="himiko.png" alt="Himiko" width="100"/>
</p>
