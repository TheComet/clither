These directories were added using the following commands:

```bash
git subtree add --squash --prefix thirdparty/flac-1.5.0 git@github.com:xiph/flac.git 1.5.0
```

```bash
git subtree add --squash --prefix thirdparty/openal-soft-1.24.3 git@github.com:kcat/openal-soft 1.24.3
```

```bash
git subtree add --squash --prefix thirdparty/speex-1.2.1 https://gitlab.xiph.org/xiph/speex release/1.2.1
cd thirdparty/speex-1.2.1
./autogen.sh
git add -f config.h.in
```
