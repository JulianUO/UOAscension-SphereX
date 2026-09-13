import re

keys = []
enum_names = []
in_table = False
with open("src/game/CServerConfig.cpp", encoding="utf-8") as f:
    mode = None
    for line in f:
        if "const CAssocReg CServerConfig::sm_szLoadKeys" in line:
            in_table = True
            continue
        if in_table and "{ nullptr" in line:
            break
        if not in_table:
            if "enum RC_TYPE" in line:
                mode = "enum"
                continue
            if mode == "enum":
                if line.strip().startswith("RC_QTY"):
                    mode = None
                    continue
                m = re.match(r"\s*(RC_[A-Z0-9_]+)", line)
                if m:
                    enum_names.append(m.group(1))
        if in_table:
            m = re.search(r'\{\s*"([A-Z0-9_]+)"', line)
            if m:
                keys.append(m.group(1))

print(f"enum count {len(enum_names)}, table count {len(keys)}")
mismatches = sum(1 for i, (en, key) in enumerate(zip(enum_names, keys)) if en[3:] != key and not (
    en == "RC_DISPLAYPERCENTAR" and key == "DISPLAYARMORASPERCENT"
    or en == "RC_MYSQLDB" and key == "MYSQLDATABASE"
    or en == "RC_MYSQLPASS" and key == "MYSQLPASSWORD"
    or en.startswith("RC_FEATURES") and en[3:] == key  # FEATURESAOS vs FEATUREAOS typo in enum name
))
print(f"index mismatches (excluding known aliases): {mismatches}")
for name in ("LOG", "MD5PASSWORDS", "MAXSHIPSACCOUNT", "INTERNALAPIPORT", "USEEXTERNALLOGIN", "LOGINSHAREDSECRET"):
    if name in keys:
        i = keys.index(name)
        print(f"{name}: table[{i}] = enum {enum_names[i]}")
