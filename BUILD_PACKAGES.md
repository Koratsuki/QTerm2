# Building QTerm2 Packages

## GitHub Actions (Automatic)
When you push a tag like `v1.0.0`, GitHub Actions will automatically build:
- Linux AppImage
- Linux .deb package
- Linux .rpm package  
- Windows ZIP with all Qt dependencies

```bash
git tag v1.0.0
git push origin v1.0.0
```

## Manual Build

### Linux AppImage
```bash
cd packaging/AppImage
./build-appimage.sh
```

### Linux .deb
```bash
cd packaging/debian
dpkg-buildpackage -us -uc -b
```

### Linux .rpm
```bash
rpmbuild -bb packaging/rpm/qterm2.spec
```

### Windows
Use the GitHub Actions workflow or build with MSVC + Qt6 and run `windeployqt`.
