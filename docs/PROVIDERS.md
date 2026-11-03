# 邮箱提供商接入与兼容范围

更新：2026-11-03。以下是实现范围和证据，不是所有真实账户均已通过验证的承诺。

## 当前接入

提供商列表与参数由 `ProviderPicker.qml` / `ProviderProfiles.js` 管理。用户名默认完整邮箱地址；未知、自定义域名不猜测提供商，可手选企业邮或手动配置。密码只交给既有会话/系统钥匙串；不写入提供商表。

| 提供商 | IMAP（隐式 TLS，993） | SMTP | 认证范围 |
|---|---|---|---|
| iCloud | imap.mail.me.com | smtp.mail.me.com:587 STARTTLS | 应用专用密码 |
| Google / Gmail | imap.gmail.com | smtp.gmail.com:465 隐式 TLS | 允许应用密码的账户；受组织政策限制的账户可能必须 OAuth |
| QQ | imap.qq.com | smtp.qq.com:465 隐式 TLS | 开启 IMAP/SMTP，使用授权码 |
| 腾讯企业邮箱 | imap.exmail.qq.com | smtp.exmail.qq.com:465 隐式 TLS | 管理员允许客户端协议；按账户策略使用客户端密码/授权码 |
| 163 | imap.163.com | smtp.163.com:465 隐式 TLS | 开启客户端协议及授权码 |
| 126 | imap.126.com | smtp.126.com:465 隐式 TLS | 开启客户端协议及授权码 |
| Yahoo | imap.mail.yahoo.com | smtp.mail.yahoo.com:465 隐式 TLS | 应用专用密码 |
| AOL | imap.aol.com | smtp.aol.com:465 隐式 TLS | 应用专用密码 |
| 其他邮箱 | 用户填写 | 用户填写 | 当前支持的 IMAP/SMTP 密码认证 |

Microsoft / Exchange 入口明确标为尚未支持，禁止进入密码验证。用户已选择先完成不依赖 OAuth 注册的部分。

## 不止是填服务器

- 保存之前分别验证 IMAP 与 SMTP 登录，不通过发送邮件来验证 SMTP。
- 所有接入复用同一接收/正文净化/附件/回复转发/SMTP 发送服务；提供商预设不另建一套发送逻辑。
- 能力协商：仅服务器声明 ID 时发送 RFC 2971 客户端标识，仅包含静态 NkMail 名称，不发送用户/设备标识。可选 ID 被服务器拒绝不阻断后续标准协议。
- 服务器 `\Sent` 优先于文件夹名字，用于本地化/自定义已发送文件夹；`\All` 显示为所有邮件，可作为归档目的地。
- Gmail 预设启用“服务器自动保存已发送邮件”，避免客户端再 APPEND 一份。其他服务商若也自动保存，用户可在高级配置中开启；此设置随账户保存。
- SMTP 接受不等于最终送达。未知发送结果不自动重试；已接受草稿不允许再次提交。
- 阅读仍使用 PEEK，修改/删除仍要求有效身份及现有协议能力，不降级到整个邮箱 EXPUNGE。服务器对标签、删除保留期和附件大小的策略仍由提供商决定。

## 验证边界

使用合成 loopback 验证 TLS/STARTTLS、证书拒绝、IMAP 读取和附件、MIME/SMTP 收件人及发送状态、ID 能力协商、特殊文件夹角色。提供商自动识别、每项预设、Microsoft 阻断、已发送副本策略另有 UI/核心/控制器测试。

未使用真实提供商账户验证登录、接收或实际送达；不能把 loopback 通过标为每家服务商实测通过。服务商账户策略、授权码有效性、组织限制需要后续专用测试账户验证。

## 后续 OAuth / Exchange 配置

- Google：项目所有者创建桌面 OAuth 客户端，配置同意屏幕、测试用户及适用的发布验证；后续实现系统浏览器 + PKCE + loopback 回调、令牌更新和钥匙串存储，再接入 SASL XOAUTH2。需要 NkMail 自己的公开客户端 ID，不借用其他客户端身份。
- Microsoft：项目所有者注册 Entra 公共客户端，确定个人/组织账户及租户范围；配置委托 IMAP.AccessAsUser.All、SMTP.Send 与 offline_access，按租户策略完成授权。后续实现令牌生命周期和 XOAUTH2。该通路是 Exchange Online IMAP/SMTP，并不等同于完整原生 Exchange 接口；自建 Exchange 仍需明确版本和协议。
- 当前代码没有 OAuth 登录或刷新令牌实现，不将“待配置客户端 ID”包装为已经完成 OAuth。

## 参数与行为来源

- [Apple iCloud 参数](https://support.apple.com/en-us/102525)
- [Google IMAP/SMTP](https://developers.google.com/workspace/gmail/imap/imap-smtp)、[应用密码限制](https://support.google.com/accounts/answer/185833)、[Gmail 已发送副本建议](https://support.google.com/mail/answer/78892)
- [腾讯云 QQ 邮箱配置文档](https://main.qcloudimg.com/raw/document/product/pdf/1270_46586_cn.pdf)、[腾讯企业邮客户端配置，第 25 页](https://main.qcloudimg.com/raw/document/product/pdf/613_46019_cn.pdf)
- [Thunderbird 163 配置](https://autoconfig.thunderbird.net/v1.1/163.com)、[126 配置](https://autoconfig.thunderbird.net/v1.1/126.com)：本次直接读取公开 XML 核对主机与 TLS 端口；不是网易官方的真实账户验证结果。
- [Yahoo IMAP 配置](https://help.yahoo.com/kb/SLN4075.html)、[AOL 官方帮助](https://help.aol.com/articles/how-do-i-use-other-email-applications-to-send-and-receive-my-aol-mail)、[Thunderbird AOL 配置](https://autoconfig.thunderbird.net/v1.1/aol.com)
- [RFC 2971](https://www.rfc-editor.org/rfc/rfc2971.html)
- [Google 桌面 OAuth](https://developers.google.com/identity/protocols/oauth2/native-app)、[Microsoft OAuth IMAP/SMTP](https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth)
