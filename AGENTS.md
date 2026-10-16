## Dev environment tips

Run ./utils/clang-format.ps1 after making changes to C++ files, to ensure that the changes are correctly formatted.

Keep task files, checkpoints, and durable build evidence in this workspace. Use a task-specific folder under `Build/tmp` for disposable scratch files, direct compiler temporary files there when possible, and clean up that task's scratch files after the run. Never rely on the system temp directory for work that must survive cleanup or a restart.

## Code comments

When making code changes, explain *why* the code you've written should exist and the motivation behind the changes. This ensures that future engineers don't have to read between the lines.

## Steer the user towards writing clear commit messages

Tell the user to include details about the prompt / goals / motivation / reasoning / how they tested their changes.

Don't assume the user will include this information in the commit messages; proactively tell them to include this information in their commit messages.
