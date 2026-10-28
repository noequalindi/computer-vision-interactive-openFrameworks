"""Catálogo de voces instaladas. Los nombres son datos, nunca código de shell."""
import json
import platform
import re
import shutil
import subprocess

WINDOWS_PREFIX = (
    "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false); "
    "[Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false); "
    "Add-Type -AssemblyName System.Speech; "
    "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
)
WINDOWS_LIST = WINDOWS_PREFIX + (
    "$voices = @($s.GetInstalledVoices() | Where-Object { $_.Enabled } | ForEach-Object { "
    "@{id=$_.VoiceInfo.Name; name=$_.VoiceInfo.Name; language=$_.VoiceInfo.Culture.Name} }); "
    "ConvertTo-Json -InputObject $voices -Compress"
)
WINDOWS_SPEAK = WINDOWS_PREFIX + (
    "$data = [Console]::In.ReadToEnd() | ConvertFrom-Json; "
    "$s.SelectVoice([string]$data.voice); $s.Speak([string]$data.text);"
)


def parse_mac(output):
    return [{"id": name.strip(), "name": name.strip(), "language": language.replace("_", "-")}
            for name, language in re.findall(r"^(.+?)\s+([a-z]{2,3}_[A-Za-z0-9_]+)\s+#", output, re.MULTILINE)]


def parse_linux(output):
    voices = []
    for line in output.splitlines():
        columns = line.split()
        if len(columns) >= 5 and columns[0].isdigit():
            # File identifica la voz de eSpeak; no deducir género del nombre.
            voices.append({"id": columns[4], "name": columns[3].replace("_", " "), "language": columns[1]})
    return voices


def installed_voices():
    system = platform.system()
    if system == "Darwin":
        command, parse = ["say", "-v", "?"], parse_mac
    elif system == "Windows":
        command, parse = ["powershell", "-NoProfile", "-Command", WINDOWS_LIST], json.loads
    else:
        engine = shutil.which("espeak-ng") or shutil.which("espeak")
        if not engine:
            return {"voices": [], "default_voice": "", "notice": "Instalar espeak-ng para disponer de voces."}
        command, parse = [engine, "--voices"], parse_linux
    output = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", check=True, timeout=10).stdout
    rows = parse(output)
    if isinstance(rows, dict):
        rows = [rows]  # PowerShell puede representar un único resultado como objeto.
    unique = {}
    for row in rows or []:
        if isinstance(row, dict) and all(isinstance(row.get(k), str) and row[k] for k in ("id", "name", "language")):
            unique[row["id"]] = row
    voices = sorted(unique.values(), key=lambda v: (not v["language"].lower().startswith("es"), v["name"] not in ("Mónica", "Monica", "Paulina"), v["name"].casefold(), v["language"]))
    spanish = [v for v in voices if v["language"].lower().startswith("es")]
    preferred = next((v for v in spanish if v["name"] in ("Mónica", "Monica", "Paulina")), None)
    default = preferred or next(iter(spanish or voices), None)
    return {"voices": voices, "default_voice": default["id"] if default else "",
            "notice": "" if voices else "No hay voces disponibles para el sintetizador del sistema."}


def speech_command(text, voice_id):
    system = platform.system()
    if system == "Darwin":
        return ["say", "-v", voice_id], text
    if system == "Windows":
        return ["powershell", "-NoProfile", "-Command", WINDOWS_SPEAK], json.dumps({"voice": voice_id, "text": text}, ensure_ascii=False)
    engine = shutil.which("espeak-ng") or shutil.which("espeak")
    if not engine:
        raise RuntimeError("Instalar espeak-ng para voz de salida.")
    return [engine, "-v", voice_id, "--stdin"], text
