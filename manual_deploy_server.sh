#!/bin/bash
echo "=== 跨平台打包研盛测试服务端 ==="

# 检测当前平台和用户
ARCH=$(uname -m)
CURRENT_USER=$(whoami)
HOME_DIR=$(eval echo ~$CURRENT_USER)

# 检测桌面环境
if [ -d "$HOME_DIR/桌面" ]; then
    DESKTOP_NAME="桌面"
elif [ -d "$HOME_DIR/Desktop" ]; then
    DESKTOP_NAME="Desktop"
else
    DESKTOP_NAME="桌面"
    echo "警告: 未找到标准桌面目录，使用默认名称'桌面'"
fi

if [ "$ARCH" = "aarch64" ]; then
    CURRENT_PLATFORM="ft"
    CURRENT_PLATFORM_NAME="飞腾ARM64"
elif [[ "$ARCH" =~ "x86_64" || "$ARCH" =~ "i386" || "$ARCH" =~ "i686" ]]; then
    CURRENT_PLATFORM="x86"
    CURRENT_PLATFORM_NAME="X86"
else
    echo "错误: 不支持的平台: $ARCH"
    exit 1
fi

echo "当前用户: $CURRENT_USER"
echo "当前平台: $ARCH ($CURRENT_PLATFORM_NAME)"
echo "桌面目录: $DESKTOP_NAME"

# 设置变量
SERVER_FT_BIN="$HOME_DIR/server/ELSKY_test_server_ft"
SERVER_X86_BIN="$HOME_DIR/server/ELSKY_test_server_x86"
DEPLOY_DIR="$HOME_DIR/$DESKTOP_NAME/ELSKY_test_server_universal"
VERSION=$(date +%Y%m%d)

# 创建部署目录
rm -rf $DEPLOY_DIR
mkdir -p $DEPLOY_DIR
cd $DEPLOY_DIR

echo "部署目录: $DEPLOY_DIR"

# 1. 复制所有平台的可执行文件
echo "复制可执行文件..."
if [ -f "$SERVER_FT_BIN" ]; then
    cp "$SERVER_FT_BIN" .
    echo "✓ 复制飞腾版本"
fi

if [ -f "$SERVER_X86_BIN" ]; then
    cp "$SERVER_X86_BIN" .
    echo "✓ 复制X86版本"
fi

# 检查是否有至少一个版本
if [ ! -f "ELSKY_test_server_ft" ] && [ ! -f "ELSKY_test_server_x86" ]; then
    echo "错误: 找不到任何平台的可执行文件"
    echo "请先运行编译脚本: ./complete_fix.sh"
    exit 1
fi

# 2. 创建智能启动脚本
cat > ELSKY_test_server << 'EOF'
#!/bin/bash
# 研盛测试服务端 - 跨平台启动脚本
echo "=== 研盛测试服务端 ==="
echo "正在检测系统平台..."

# 检测架构
ARCH=$(uname -m)
APP_NAME=""

if [ "$ARCH" = "aarch64" ]; then
    echo "✓ 检测到飞腾ARM64平台"
    APP_NAME="ELSKY_test_server_ft"
elif [[ "$ARCH" =~ "x86_64" ]]; then
    echo "✓ 检测到X86_64平台"
    APP_NAME="ELSKY_test_server_x86"
elif [[ "$ARCH" =~ "i386" || "$ARCH" =~ "i686" ]]; then
    echo "✓ 检测到X86平台"
    APP_NAME="ELSKY_test_server_x86"
else
    echo "✗ 错误: 不支持的平台架构: $ARCH"
    echo ""
    echo "支持的平台:"
    echo "  - x86_64, i386, i686 (兆芯、海光、Intel、AMD等)"
    echo "  - aarch64 (飞腾ARM64平台)"
    echo ""
    echo "当前目录可执行文件:"
    ls -la ELSKY_test_server_* 2>/dev/null || echo "  无"
    exit 1
fi

# 检查可执行文件是否存在
if [ ! -f "$APP_NAME" ]; then
    echo "✗ 错误: 找不到对应平台的可执行文件: $APP_NAME"
    echo ""
    echo "当前目录文件:"
    ls -la
    exit 1
fi

# 设置执行权限
chmod +x "$APP_NAME"

echo "启动服务端..."
echo "平台: $ARCH"
echo "程序: $APP_NAME"
echo "默认端口: 8888"
echo "================================="

# 启动程序
exec "./$APP_NAME" "$@"
EOF

chmod +x ELSKY_test_server

# 3. 创建库目录并收集依赖库（当前平台）
mkdir -p lib
echo "收集依赖库..."
if [ -f "ELSKY_test_server_${CURRENT_PLATFORM}" ]; then
    ldd "ELSKY_test_server_${CURRENT_PLATFORM}" | grep "=> /" | awk '{print $3}' | while read lib; do
        if [ -f "$lib" ]; then
            echo "复制: $(basename $lib)"
            cp "$lib" lib/
        fi
    done
fi

# 4. 创建桌面快捷方式模板（使用变量占位符）
cat > ELSKY_test_server.desktop.template << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=研盛测试服务端(跨平台)
Comment=电源管理和网络唤醒服务器 - 支持飞腾和X86平台
Exec=sh -c "cd \"$HOME/DESKTOP_PLACEHOLDER/ELSKY_test_server_universal\" && ./ELSKY_test_server"
Icon=network-server
Categories=Utility;
Terminal=false
StartupNotify=true
X-Platform-Support=ft,x86
EOF

# 5. 创建安装脚本（动态生成桌面快捷方式）
cat > install.sh << 'EOF'
#!/bin/bash
echo "=== 安装研盛测试服务端(跨平台版) ==="

# 获取当前目录
INSTALL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CURRENT_USER=$(whoami)
HOME_DIR=$(eval echo ~$CURRENT_USER)

# 检测桌面目录
DESKTOP_DIR=""
DESKTOP_NAME=""
if [ -d "$HOME_DIR/桌面" ]; then
    DESKTOP_DIR="$HOME_DIR/桌面"
    DESKTOP_NAME="桌面"
    echo "✓ 检测到中文桌面目录: 桌面"
elif [ -d "$HOME_DIR/Desktop" ]; then
    DESKTOP_DIR="$HOME_DIR/Desktop"
    DESKTOP_NAME="Desktop"
    echo "✓ 检测到英文桌面目录: Desktop"
else
    echo "⚠ 警告: 未找到桌面目录，尝试创建中文桌面目录"
    DESKTOP_DIR="$HOME_DIR/桌面"
    DESKTOP_NAME="桌面"
    mkdir -p "$DESKTOP_DIR"
fi

# 检测平台
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    PLATFORM_NAME="飞腾ARM64"
elif [[ "$ARCH" =~ "x86_64" || "$ARCH" =~ "i386" || "$ARCH" =~ "i686" ]]; then
    PLATFORM_NAME="X86"
else
    PLATFORM_NAME="未知"
fi

echo "系统平台: $ARCH ($PLATFORM_NAME)"
echo "当前用户: $CURRENT_USER"
echo "桌面目录: $DESKTOP_NAME"

# 动态生成桌面快捷方式
echo "生成桌面快捷方式..."
cat > "$INSTALL_DIR/ELSKY_test_server.desktop" << DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=研盛测试服务端(跨平台)
Comment=电源管理和网络唤醒服务器 - 支持飞腾和X86平台
Exec=sh -c "cd \"\$HOME/$DESKTOP_NAME/ELSKY_test_server_universal\" && ./ELSKY_test_server"
Icon=network-server
Categories=Utility;
Terminal=false
StartupNotify=true
X-Platform-Support=ft,x86
DESKTOP_EOF

# 复制桌面快捷方式
if [ -d "$DESKTOP_DIR" ]; then
    cp "$INSTALL_DIR/ELSKY_test_server.desktop" "$DESKTOP_DIR/"
    chmod +x "$DESKTOP_DIR/ELSKY_test_server.desktop"
    echo "✓ 桌面快捷方式已创建: $DESKTOP_NAME/ELSKY_test_server.desktop"
    
    # 尝试更新桌面图标缓存
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$HOME_DIR/.local/share/applications"
        echo "✓ 更新桌面图标缓存"
    fi
else
    echo "⚠ 警告: 桌面目录不存在，无法创建快捷方式"
fi

# 检查可执行文件
if [ -f "$INSTALL_DIR/ELSKY_test_server_ft" ]; then
    echo "✓ 飞腾版本: 已包含"
fi
if [ -f "$INSTALL_DIR/ELSKY_test_server_x86" ]; then
    echo "✓ X86版本: 已包含"
fi

echo ""
echo "安装完成！"
echo "支持的平台: 飞腾ARM64, X86(兆芯/海光/Intel/AMD)"
echo "当前平台: $PLATFORM_NAME"
echo "桌面快捷方式: $DESKTOP_NAME/ELSKY_test_server.desktop"
echo ""
echo "启动方式:"
echo "1. 双击桌面图标'研盛测试服务端(跨平台)'"
echo "2. 或进入目录运行: ./ELSKY_test_server"
echo ""
echo "如果桌面图标无法启动，请尝试:"
echo "1. 右键桌面图标 -> 允许启动"
echo "2. 或直接运行: ./ELSKY_test_server"
EOF
chmod +x install.sh

# 6. 创建使用说明
cat > README.txt << EOF
研盛测试服务端 - 跨平台版使用说明

一、支持平台：
  ✓ 飞腾ARM64平台 (aarch64)
  ✓ X86平台 (x86_64, i386, i686)
    - 兆芯平台
    - 海光平台  
    - Intel平台
    - AMD平台

二、安装方法：
1. 解压后进入目录
2. 运行安装脚本: ./install.sh
3. 双击桌面图标启动

三、智能启动：
程序会自动检测系统平台并启动对应版本，无需手动选择。

四、服务端配置：
1. 默认监听端口: 8888
2. 支持多客户端连接
3. 自动管理网络唤醒配置

五、文件说明：
  ELSKY_test_server      - 智能启动脚本(自动选择平台)
  ELSKY_test_server_ft   - 飞腾ARM64版本
  ELSKY_test_server_x86  - X86版本
  install.sh            - 安装脚本
  lib/                  - 依赖库目录

六、故障排除：
如果无法启动，请确保：
1. 系统架构受支持
2. 端口8888未被占用
3. 文件有执行权限

技术支持：唐贵宝

注意: 此包由用户 $CURRENT_USER 在 $CURRENT_PLATFORM_NAME 平台打包
打包时间: $(date +"%Y-%m-%d %H:%M:%S")
EOF

# 7. 创建版本信息文件
cat > version.info << EOF
产品名称: 研盛测试服务端
版本: 跨平台版
编译日期: $(date +%Y-%m-%d)
打包用户: $CURRENT_USER
打包平台: $CURRENT_PLATFORM_NAME
支持平台: ft,x86
包含版本:
$(ls ELSKY_test_server_* 2>/dev/null | while read file; do echo "  - $file"; done)
EOF

# 8. 设置文件权限
chmod +x ELSKY_test_server_* 2>/dev/null

# 9. 打包成压缩文件
cd ..
tar -czf "ELSKY_test_server_universal_${VERSION}.tar.gz" "$(basename $DEPLOY_DIR)"

echo "=== 跨平台打包完成 ==="
echo "部署包: $(pwd)/ELSKY_test_server_universal_${VERSION}.tar.gz"
echo "大小: $(du -h ELSKY_test_server_universal_${VERSION}.tar.gz | cut -f1)"
echo "包含平台版本:"
if [ -f "$SERVER_FT_BIN" ]; then echo "  ✓ 飞腾ARM64版本"; fi
if [ -f "$SERVER_X86_BIN" ]; then echo "  ✓ X86版本"; fi
echo ""
echo "部署步骤："
echo "1. 将压缩包复制到目标电脑"
echo "2. 右键解压到桌面"
echo "3. 进入解压后的目录"  
echo "4. 运行: ./install.sh  (这一步会动态创建正确的桌面快捷方式)"
echo "5. 双击桌面图标启动"
echo ""
echo "注意: 此包包含多平台版本，会自动检测系统平台运行对应版本"