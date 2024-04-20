import gdb

class lake(gdb.Command):
    def __init__(self):
        super(lake, self).__init__("lake", gdb.COMMAND_USER)

    def invoke(self, arg, tty):
        try:
            gdb.execute(f"shell ./lake {arg}", from_tty=True)
        except Exception as e:
            print(f"{self.__class__.__name__} error: {e}")
lake()
