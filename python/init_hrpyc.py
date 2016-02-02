import os
if os.environ.get('DEV'):
    import hrpyc
    conn, hou = hrpyc.import_remote_module()
else:
    import hou

