#!/bin/bash
echo "=== 跨平台编译修复脚本 ==="

# 检测当前平台
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    PLATFORM="ft"
    PLATFORM_NAME="飞腾ARM64"
elif [[ "$ARCH" =~ "x86_64" || "$ARCH" =~ "i386" || "$ARCH" =~ "i686" ]]; then
    PLATFORM="x86"
    PLATFORM_NAME="X86"
else
    PLATFORM="unknown"
    PLATFORM_NAME="未知平台"
fi

# 检测具体飞腾型号
if [ "$ARCH" = "aarch64" ]; then
    CPU_MODEL=$(grep -i "model name" /proc/cpuinfo | head -1 | tr -d ' ' | cut -d: -f2)
    if echo "$CPU_MODEL" | grep -q "FT-2000"; then
        PLATFORM="ft2000"
        PLATFORM_NAME="飞腾D2000"
        echo "✓ 检测到飞腾D2000平台"
        
        # D2000特定编译选项
        export CXXFLAGS="$CXXFLAGS -march=armv8-a -mtune=cortex-a72"
    elif echo "$CPU_MODEL" | grep -q "FT-2500"; then
        PLATFORM="ft3000" 
        PLATFORM_NAME="飞腾D3000"
        echo "✓ 检测到飞腾D3000平台"
    else
        PLATFORM="ft"
        PLATFORM_NAME="飞腾ARM64(通用)"
    fi
fi

echo "检测到平台: $ARCH ($PLATFORM_NAME)"

# 1. 只清理必要的编译文件，不删除目录
echo "清理编译文件..."
cd /home/it0
find . -name "Makefile" -delete
find . -name "*.o" -delete
find . -name "moc_*" -delete
find . -name "ELSKY_test_*" -delete

# 2. 修复头文件包含
echo "修复头文件包含..."
# 这些修改已经在上面手动完成了

# 3. 编译服务端
echo "编译服务端..."
cd /home/D200SE/桌面/Elsky_server/server
qmake server.pro
if make -j2; then
    # 重命名可执行文件包含平台信息
    mv ELSKY_test_server ELSKY_test_server_${PLATFORM}
    echo "✓ 服务端编译成功: ELSKY_test_server_${PLATFORM}"
else
    echo "✗ 服务端编译失败"
    exit 1
fi

# 4. 编译客户端
echo "编译客户端..."
cd /home/it0/client
qmake client.pro
if make -j2; then
    # 重命名可执行文件包含平台信息
    mv ELSKY_test_client ELSKY_test_client_${PLATFORM}
    echo "✓ 客户端编译成功: ELSKY_test_client_${PLATFORM}"
else
    echo "✗ 客户端编译失败"
    exit 1
fi

# 5. 检查结果
cd /home/D200SE/桌面/Elsky_server/
if [ -f server/ELSKY_test_server_${PLATFORM} ] && [ -f client/ELSKY_test_client_${PLATFORM} ]; then
    echo "✓ 编译完全成功!"
    echo "服务端: $(pwd)/server/ELSKY_test_server_${PLATFORM}"
    echo "客户端: $(pwd)/client/ELSKY_test_client_${PLATFORM}"
    echo "平台: $PLATFORM_NAME"
else
    echo "✗ 编译失败"
    exit 1
fi

# 6. 创建跨平台启动脚本
echo "创建跨平台启动脚本..."
create_universal_script() {
    local target_dir=$1
    local app_name=$2
    local display_name=$3
    
    cat > ${target_dir}/start_${app_name}.sh << EOF
#!/bin/bash
echo "=== 启动 ${display_name} ==="
echo "检测系统架构..."

ARCH=\$(uname -m)
APP_NAME=""

if [ "\$ARCH" = "aarch64" ]; then
    echo "检测到飞腾ARM64平台"
    APP_NAME="${app_name}_ft"
elif [[ "\$ARCH" =~ "x86_64" || "\$ARCH" =~ "i386" || "\$ARCH" =~ "i686" ]]; then
    echo "检测到X86平台"
    APP_NAME="${app_name}_x86"
else
    echo "错误: 不支持的平台架构: \$ARCH"
    echo "支持的平台:"
    echo "  - x86_64, i386, i686 (X86平台)"
    echo "  - aarch64 (飞腾ARM64平台)"
    exit 1
fi

if [ -f "\$APP_NAME" ]; then
    echo "启动: \$APP_NAME"
    chmod +x "\$APP_NAME"
    ./"\$APP_NAME"
else
    echo "错误: 找不到可执行文件 \$APP_NAME"
    echo "当前目录文件列表:"
    ls -la ${app_name}_*
    exit 1
fi
EOF
    chmod +x ${target_dir}/start_${app_name}.sh
}

# 为客户端和服务端创建启动脚本
create_universal_script "/home/it0/client" "ELSKY_test_client" "研盛测试客户端"
create_universal_script "/home/it0/server" "ELSKY_test_server" "研盛测试服务端"

echo "✓ 跨平台启动脚本创建完成"
