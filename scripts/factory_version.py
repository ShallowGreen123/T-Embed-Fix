Import("env")

import subprocess


def git_commit_hash(project_dir):
    try:
        return subprocess.check_output(
            ["git", "-C", project_dir, "rev-parse", "--short=12", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


project_dir = env.subst("$PROJECT_DIR")
commit_hash = git_commit_hash(project_dir)
env.Append(CPPDEFINES=[("FACTORY_COMMIT_HASH", commit_hash)])
print("FACTORY_COMMIT_HASH: {}".format(commit_hash))
