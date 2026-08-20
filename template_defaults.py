# template_defaults.py - ship packaged defaults as .template.json so user
# edits are never clobbered by an update (new package templates only seed
# first-time installs; existing user data always wins).
import io

def patch(path, subs):
    src = io.open(path, encoding='utf-8').read()
    n = 0
    for old, new in subs:
        if old in src:
            src = src.replace(old, new)
            n += 1
        else:
            print('MISS in', path, ':', old[:70])
    io.open(path, 'w', encoding='utf-8').write(src)
    print(path, '->', n, 'patched')

# 1) assemble_one.bat: ship defaults as .template.json
patch(r'D:\Project\EndfieldBreastMotion\assemble_one.bat', [
    (r'copy /y "%ROOT%SecondaryMotion\data\characters.default.json" "%STAGE%\data\" >nul',
     r'copy /y "%ROOT%SecondaryMotion\data\characters.default.json" "%STAGE%\data\characters.default.template.json" >nul'),
    (r'copy /y "%ROOT%SecondaryMotion\presets\Default.json" "%STAGE%\presets\" >nul',
     r'copy /y "%ROOT%SecondaryMotion\presets\Default.json" "%STAGE%\presets\Default.template.json" >nul'),
    (r'if exist "%ROOT%SecondaryMotion\presets\User.json" copy /y "%ROOT%SecondaryMotion\presets\User.json" "%STAGE%\presets\" >nul',
     r'if exist "%ROOT%SecondaryMotion\presets\User.json" copy /y "%ROOT%SecondaryMotion\presets\User.json" "%STAGE%\presets\User.template.json" >nul'),
    (r'python "%ROOT%zh_names.py" "%STAGE%\data\characters.default.json" "%STAGE%"',
     r'python "%ROOT%zh_names.py" "%STAGE%\data\characters.default.template.json" "%STAGE%"'),
])

# 2) App.xaml.cs FirstRunSetup: copy from .template.json sources
patch(r'D:\Project\EndfieldBreastMotion\Manager\App.xaml.cs', [
    (r'CopyIfMissing(Path.Combine(ManagerDir, "data", "characters.default.json"),' + "\n" +
     r'                          Path.Combine(sm, "data", "characters.default.json"));',
     r'CopyIfMissing(Path.Combine(ManagerDir, "data", "characters.default.template.json"),' + "\n" +
     r'                          Path.Combine(sm, "data", "characters.default.json"));'),
    (r'CopyIfMissing(Path.Combine(ManagerDir, "presets", "Default.json"),' + "\n" +
     r'                          Path.Combine(sm, "presets", "Default.json"));',
     r'CopyIfMissing(Path.Combine(ManagerDir, "presets", "Default.template.json"),' + "\n" +
     r'                          Path.Combine(sm, "presets", "Default.json"));'),
    (r'CopyIfMissing(Path.Combine(ManagerDir, "presets", "User.json"),' + "\n" +
     r'                          Path.Combine(sm, "presets", "User.json"));',
     r'CopyIfMissing(Path.Combine(ManagerDir, "presets", "User.template.json"),' + "\n" +
     r'                          Path.Combine(sm, "presets", "User.json"));'),
])
