"""
test_jobs.py — Tests automatiques du job control d'OCSH (jalons 3 et 6).

Le shell est lance dans un pseudo-terminal (pty), comme dans un vrai
terminal, pour pouvoir lui envoyer Ctrl+C et Ctrl+Z. Le script verifie :
  1. Ctrl+Z suspend la commande (etat T) et affiche "Stopped", $? = 148
  2. bg relance le job en arriere-plan
  3. fg le ramene au premier plan ; Ctrl+C le tue, le shell survit ($? = 130)
  4. Ctrl+C au prompt ne tue pas un job d'arriere-plan
  5. fg + Ctrl+Z + fg d'un job tue : pas de blocage
  6. un pipeline entier peut etre suspendu, repris et tue
  7. fg / bg sans job affichent une erreur
  8. quit termine les jobs suspendus (pas d'orphelin)

Utilisation (depuis la racine du projet, apres make) :
    python3 code/tests/test_jobs.py
Chaque test affiche PASS ou FAIL ; la derniere ligne donne le resultat global.
"""

import os, pty, time, select, re, sys, signal

# Chemin du shell compile : ce fichier est dans code/tests/, ocsh est a la
# racine du projet (deux dossiers au-dessus). Fonctionne quel que soit
# le dossier depuis lequel on lance le test.
OCSH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ocsh")

# Lance ocsh dans un pseudo-terminal ; retourne (pid du shell, descripteur)
def spawn():
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(OCSH, [OCSH])
    return pid, fd

# Lit tout ce que le shell affiche pendant t secondes
def read(fd, t=0.5):
    out = b""
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try: d = os.read(fd, 4096)
            except OSError: break
            if not d: break
            out += d
    return out.decode(errors="replace").replace("\r", "")

# Envoie du texte au shell (\x03 = Ctrl+C, \x1a = Ctrl+Z) et lit la reponse
def send(fd, s, t=0.6):
    os.write(fd, s.encode()); return read(fd, t)

# Etat d'un processus lu dans /proc : S/R = actif, T = suspendu,
# Z = zombie, "gone" = n'existe plus
def state(pidstr):
    try:
        return open(f"/proc/{pidstr}/stat").read().split(")")[1].split()[0]
    except Exception: return "gone"

# Affiche PASS ou FAIL pour un test ; ok passe a False au premier echec
ok = True
def check(name, cond, info=""):
    global ok
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else "  -> " + info))
    ok &= bool(cond)

pid, fd = spawn(); read(fd, 0.5)

# 1. Ctrl+Z sur un premier plan
out = send(fd, "sleep 30\n", 0.6)
out = send(fd, "\x1a", 0.8)
print("--- apres Ctrl+Z:", repr(out))
check("Ctrl+Z affiche Stopped", "Stopped" in out and "sleep 30" in out, out)
out = send(fd, "echo code=$?\n")
check("code de retour 148 apres Ctrl+Z", "code=148" in out, out)
out = send(fd, "jobs\n")
check("jobs montre Stopped", "Stopped" in out, out)

# PID du sleep
sp = os.popen("pgrep -f 'sleep 30' | head -1").read().strip()
check("le sleep est bien suspendu (etat T)", state(sp) == "T", state(sp))

# 2. bg
out = send(fd, "bg\n")
print("--- apres bg:", repr(out))
check("bg affiche le job", "sleep 30 &" in out, out)
time.sleep(0.3)
check("le sleep tourne a nouveau (etat S)", state(sp) in ("S", "R"), state(sp))
out = send(fd, "jobs\n")
check("jobs montre Running", "Running" in out, out)

# 3. fg puis Ctrl+C
out = send(fd, "fg\n", 0.6)
check("fg reaffiche la commande", "sleep 30" in out, out)
out = send(fd, "\x03", 0.8)
print("--- apres Ctrl+C:", repr(out))
check("Ctrl+C tue le job, le shell survit", state(sp) == "gone" and "ocsh>" in out, out + state(sp))
out = send(fd, "echo vivant=$?\n")
check("shell vivant, code 130", "vivant=130" in out, out)

# 4. job de fond + Ctrl+C ne le tue pas
out = send(fd, "sleep 40 &\n", 0.5)
sp2 = os.popen("pgrep -f 'sleep 40' | head -1").read().strip()
out = send(fd, "\x03", 0.5)
check("Ctrl+C (au prompt) ne tue pas le job de fond", state(sp2) in ("S","R"), state(sp2))

# 5. fg d'un job de fond, puis Ctrl+Z, puis fg, jusqu'a la fin
out = send(fd, "fg %1\n", 0.5)
out = send(fd, "\x1a", 0.8)
check("fg puis Ctrl+Z : Stopped", "Stopped" in out, out)
check("etat T", state(sp2) == "T", state(sp2))
os.kill(int(sp2), signal.SIGKILL); time.sleep(0.2)
out = send(fd, "fg\n", 0.8)
out = send(fd, "echo apres=$?\n")
check("fg d'un job tue : pas de blocage", "apres=" in out, out)

# 6. pipeline suspendu et repris
out = send(fd, "sleep 30 -> cat\n", 0.5)
out = send(fd, "\x1a", 0.8)
check("pipeline : Ctrl+Z", "Stopped" in out and "sleep 30 -> cat" in out, out)
out = send(fd, "bg\n", 0.4)
out = send(fd, "jobs\n")
check("pipeline repris en fond", "Running" in out, out)
out = send(fd, "fg\n", 0.4); out = send(fd, "\x03", 0.8)
out = send(fd, "echo pipe=$?\n")
check("pipeline tue par Ctrl+C", "pipe=130" in out, out)

# 7. fg/bg sans job
out = send(fd, "fg\n"); check("fg sans job : erreur", "no such job" in out, out)
out = send(fd, "bg\n"); check("bg sans job : erreur", "no stopped job" in out, out)

# 8. job suspendu a la sortie : nettoye
out = send(fd, "sleep 55\n", 0.4); out = send(fd, "\x1a", 0.6)
sp3 = os.popen("pgrep -f 'sleep 55' | head -1").read().strip()
out = send(fd, "quit\n", 0.8)
time.sleep(0.3)
check("quit : le job suspendu est termine (pas d'orphelin)", state(sp3) in ("gone","Z"), state(sp3))
print("RESULT:", "TOUT PASSE" if ok else "ECHECS")
