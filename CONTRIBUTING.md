# Contributing to Ricky

Thanks for helping improve Ricky.

## Development setup

You need Linux on x86-64, a C compiler, GNU Make, and the system assembler and
linker. Build the compiler with:

```sh
make
```

## Making changes

1. Keep the language and compiler intentionally small and readable.
2. Add or update an end-to-end test for behavior changes.
3. Run the complete suite:

   ```sh
   make clean
   make test
   ```

4. Keep generated files out of commits. The compiler executable, object files,
   assembly output, IDE settings, and analyzer reports are ignored.
5. Update `README.md` when syntax, behavior, requirements, or limitations change.

## Commit and pull-request guidance

- Use a short, imperative commit subject such as `test: cover nested calls`.
- Keep each pull request focused on one change.
- Explain what changed and include the `make test` result.
- Do not include unrelated formatting or generated build artifacts.
