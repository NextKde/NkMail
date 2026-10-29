# NkMail 进展 / Progress

此目录只包含经过检查的 **NkMail 自身截图与合成数据**，不包含真实账号、邮件、密码、原始日志或构建产物。截图证明界面实际运行，不等同于真实服务商收发验收。

## 2026-10-29 · 工作流与窗口体验

[第二轮说明与新截图](2026-10-29-workflow-window/README.md)

列宽拖动与窗口记忆、真实控制器集成测试、离线/换代分页修复、钥匙环按数据目录隔离，以及回复收件人/附件/草稿工作流回归。真实服务商读取尚未通过，未执行外部发信。

![NkMail 深色写信](2026-10-29-workflow-window/compose-dark.png)

## 2026-10-29 · 首个可运行里程碑

[完整说明与六张截图](2026-10-29-first-runnable/README.md)

- Qt Quick / canonical Kos.Ui 实际窗口，参考 macOS Mail 的三栏信息层级。
- mailio IMAP/SMTP/MIME 后端、TLS/PEEK 补丁、SQLite 缓存与本地草稿。
- 最终集成构建成功，CTest **3/3** 通过：核心、UI 和真实 loopback 协议测试。
- 真实服务商兼容性尚未验证；未进行外部 SMTP 投递或邮箱写操作。

![NkMail 浅色收件箱](2026-10-29-first-runnable/inbox-light.png)

![NkMail 深色收件箱](2026-10-29-first-runnable/inbox-dark.png)
