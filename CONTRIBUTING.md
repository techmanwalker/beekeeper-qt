# Contributing to beekeeper-qt

I welcome contributions of all kinds: bugfixes, features, documentation improvements, translations, or packaging work.

## Code style

- Use **snake_case** everywhere (functions, variables, file names).  
- Opening braces `{` always go on their own line.   
- Avoid camelCase or PascalCase unless working on legacy code not yet refactored. **snake_case** is preffered for new code.

## Pull requests (PRs)

- Keep commits clear and scoped.  
- Write commit messages in imperative form:  
  - Good: `Add systemd autostart helper`  
  - Bad: `Added systemd autostart helper`  
- Squash small “fix typo” or “oops” commits before opening a PR.  
- Reference issues if relevant: `Fixes #13`.  

## Documentation

- Documentation lives in `docs/`.  
- Doxygen is used for API docs (`make doc` target).  
- README must always be kept up to date when adding major features.

## License

By contributing, you agree that your code will be licensed under the same license as the project: **AGPLv3**.
