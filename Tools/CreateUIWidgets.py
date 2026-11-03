"""Create the six Widget Blueprints parented to the C++ UI classes.

Unreal 5.8 does not let Python read a Widget Blueprint's widget tree, so this
script calls the editor module that builds and saves the assets.
"""

import os
import traceback

import unreal


def log(message):
    unreal.log("[CreateUIWidgets] " + message)


def warn(message):
    unreal.log_warning("[CreateUIWidgets] " + message)


def main():
    library = getattr(unreal, "CreateUIWidgetsLibrary", None)
    if library is None:
        raise RuntimeError("CreateUIWidgetsLibrary is not loaded. Compile the editor module first.")
    if not library.create_project_widgets():
        raise RuntimeError("CreateProjectWidgets failed. See the Output Log.")
    log("All widget blueprints were created.")


if __name__ == "__main__":
    status = "failed"
    try:
        main()
        status = "ok"
    except Exception:
        unreal.log_error("[CreateUIWidgets] " + traceback.format_exc())
        raise
    finally:
        marker = os.path.join(unreal.Paths.project_saved_dir(), "ui_widgets_created.txt")
        try:
            with open(marker, "w", encoding="utf-8") as handle:
                handle.write(status + "\n")
        except Exception as exc:
            warn("Could not write marker file: " + str(exc))
