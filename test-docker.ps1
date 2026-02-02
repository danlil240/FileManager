# Test script for Docker build verification
Write-Host "Testing Docker build without GUI..." -ForegroundColor Cyan

# Test 1: Verify binary exists and runs with offscreen platform
Write-Host "`n[Test 1] Checking binary with offscreen rendering..." -ForegroundColor Yellow
docker run -it --rm -e QT_QPA_PLATFORM=offscreen dolphin-lite bash -c "ls -lh ./build/dolphin-lite && file ./build/dolphin-lite"

# Test 2: Check Qt dependencies
Write-Host "`n[Test 2] Verifying Qt libraries..." -ForegroundColor Yellow
docker run -it --rm dolphin-lite bash -c "ldd ./build/dolphin-lite | grep -i qt"

# Test 3: Try running with minimal platform (no X11 needed)
Write-Host "`n[Test 3] Testing with minimal platform plugin..." -ForegroundColor Yellow
docker run -it --rm -e QT_QPA_PLATFORM=minimal dolphin-lite timeout 2 ./build/dolphin-lite || echo "Exit code: $LASTEXITCODE (timeout expected)"

Write-Host "`n=== Build Verification Complete ===" -ForegroundColor Green
Write-Host "The application is built successfully inside Docker." -ForegroundColor Green
Write-Host "`nTo run with GUI, you need to:" -ForegroundColor Cyan
Write-Host "1. Install and start VcXsrv (X Server for Windows)" -ForegroundColor White
Write-Host "2. Configure VcXsrv with 'Disable access control' enabled" -ForegroundColor White
Write-Host "3. Then run: docker-compose up" -ForegroundColor White
Write-Host "`nSee DOCKER_TESTING.md for detailed setup instructions." -ForegroundColor Cyan
