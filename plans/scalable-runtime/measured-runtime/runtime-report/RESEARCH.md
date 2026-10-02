# Research: runtime-report

The rules for summarizing launches and deciding that a change is clear are settled at the milestone (plans/scalable-runtime/measured-runtime/RESEARCH.md): each launch is summarized by its median, a report gives the median of those medians with their least and greatest, and a change is clear only when the two reports' launches do not overlap. The questions here are how the script orders its launches and how it hands paths to Windows executables.

## In what order should one report's launches run?

Abedi and Brecht's randomized multiple interleaved trials (RMIT) answer this for experiments whose machine changes under them. They run the experiment as consecutive trials. Every alternative runs once in each trial, and the order within a trial is shuffled. Running every launch of one alternative together, as hyperfine does for one command, lets a slow stretch of the machine, such as a background job or a hotter chip, land wholly on that alternative. Interleaving spreads the stretch over all of them. Shuffling within a trial also breaks any fixed position effect, such as one launch always following one that heated the chip.

A report here runs every stress park on both builds, and the alternatives it compares are not inside one report: they are two reports, made before and after a change. So rounds suit it: in each round, every park runs once on each build. Each park's launches then span the whole report, and any drift during the report shows in that park's spread rather than in its median alone. Shuffling within a round would turn a fixed position effect into extra spread. A fixed order instead puts the same position effect in both reports, where it cancels in the comparison. Comparing two trees inside one run, which is where RMIT's shuffling earns its place, is the milestone's paired-runs candidate.

Rejected: blocks, all of one park's launches together. A drift during the report would move some parks' medians and not others', and would not show in their spread. Rejected: shuffling the order within each round. It makes the order differ between the two reports being compared, so a fixed position effect stops cancelling and widens both spreads instead.

Sources: https://cs.uwaterloo.ca/~brecht/papers/icpe-rmit-2017.pdf — trials that run every alternative once, shuffled within each trial; https://www.ifi.uzh.ch/dam/jcr:326f9543-d719-4e8c-a27e-d5a5cace1abf/emse_smb_cloud.pdf — RMIT as practice for comparing benchmark results on variable machines.

## How does a WSL script hand files to the Windows executables?

A Windows executable launched from WSL starts in the Windows form of the shell's working directory when that directory is on a mounted Windows drive, as the repository is. Relative paths under it therefore name the same file on both sides. tpj_bench already runs on tests/parks/stress/ this way. An absolute WSL path such as /home/... or /tmp/... means nothing to a Windows program, and the supported translation is wslpath -w, which gives a drive path or a \\wsl.localhost\ share. Anything the executable writes to standard output reaches the shell, so the shell, not the executable, can write the output file, wherever it lives. Windows programs write a carriage return before each line feed in text mode, so a reader of their output strips it, as cross-build-check.sh does with tr.

So the script launches tpj_bench and the report tool with relative paths for files inside the repository, converts any path the user gives with wslpath -w before handing it to a Windows program, lets the shell write every output file, and the report tool's readers ignore a carriage return at a line's end.

Rejected: having the executables write their output files themselves. A path the user gives on the WSL side would need translating for every write, while the shell writes anywhere. Rejected: running the report tool on Linux. The script then needs a Linux build as well, and nothing about summarizing needs to run natively.

Sources: https://learn.microsoft.com/en-us/windows/dev-environment/wsl-interop — Windows executables from WSL, working directory, and wslpath.
