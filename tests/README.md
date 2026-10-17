# 体感迷宫模型验证

`src/maze_selftest.c` 为启动时运行的24项板上自检，入口声明在 `maze_selftest.h`。`tests/maze_model_host.c` 只用于主机复用同一用例，位于 src 外，不参与固件构建，也不会产生第二个板端 main。

板上必须通过课程的 `python scripts/build_firmware.py` 编译，由学生烧录并留存完整 PASS/FAIL 日志。主机测试只能补充逻辑检查，不能替代板端调度、传感器、UI和真实体验。

主机（有cc和sanitizer的Linux/WSL环境，在仓库根目录）：

```sh
mkdir -p build
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc \
  tests/maze_model_host.c src/maze_selftest.c src/maze_game.c \
  -lm -o build/maze_model_host
./build/maze_model_host
```

期望24项PASS且末尾 `summary PASS failures=0`，退出码0。失败时保留原输出，不修改判据凑通过。契约和测试步骤见 `docs/guides/体感迷宫-方案A接口与首轮自检.md`；六组用例TC-301～TC-306在实现前已写入测试计划。
