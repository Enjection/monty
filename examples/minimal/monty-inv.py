from invoke import task

""" This script is loaded by `inv` to define additional tasks for this area.
    The example below defines an "all" task. Uncomment and adjust to taste.
"""

@task(default=True)
def all(c):
    """This is a demo "all" task, defined in monty-inv.py"""

    print("""
        See the PyInvoke site for how tasks work: https://www.pyinvoke.org
        One quick option is to simply make "all" dependent on some of the
        other tasks. Since it is the default, running "inv" without any args
        will then run each of the dependent tasks in sequence. For example:

            @task(native, test, python)
            def all(c):
                pass

        With this setup, "inv" will compile a native build and run all the
        C++ & Python tests. Adjust as needed to your own preferred workflow.
    """)