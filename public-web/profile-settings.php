<?php if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; } ?>
<div class="profile-settings">
    <h3>Player settings</h3>
    <fieldset class="avatar-picker">
        <legend>Avatar</legend>
        <div class="avatar-grid">
            <?php foreach(['Bronze Sentinel','Cave Oracle','Jade Alchemist','Violet Sorcerer','Crimson Gargoyle','Drowned Knight','Goblin Engineer','Golden Mask','Bone Archivist','Spore Shaman','Owl Seer','Bat Ranger','Lava Construct','Dragon Scholar','Moss Apothecary'] as $index=>$avatar): ?>
                <label class="avatar-option">
                    <input type="radio" name="avatar" value="<?= $index+1 ?>">
                    <span class="profile-avatar preset-avatar" data-avatar="<?= $index+1 ?>" aria-hidden="true"></span>
                    <span><?= web_escape($avatar) ?></span>
                </label>
            <?php endforeach; ?>
        </div>
    </fieldset>
    <details>
        <summary>Change email</summary>
        <p id="profile-email"></p>
        <form method="post" id="profile-email-form">
            <label>New email<input name="email" type="email" required maxlength="254" autocomplete="email"></label>
            <label>Current password<input name="current_password" type="password" required maxlength="128" autocomplete="current-password"></label>
            <button>Send verification code</button>
        </form>
        <form method="post" id="profile-email-verify" hidden>
            <label>Code sent to your new email<input name="code" required inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6}" maxlength="6"></label>
            <button>Verify new email</button>
        </form>
    </details>
    <details id="dedicated-profile">
        <summary>Dedicated Servers</summary>
        <p>Link a public host to this account. Administrators can start lobbies and advance the map.</p>
        <form id="dedicated-host-form" method="post">
            <label>Server name<input name="name" required maxlength="64"></label>
            <button>Create Server Credential</button>
        </form>
        <div id="dedicated-credential-panel" hidden>
            <p>Copy the credential now. On the server, run the dedicated supervisor with <code>--link</code> and paste it when prompted.</p>
            <label>Server credential<input id="dedicated-credential" readonly autocomplete="off"></label>
        </div>
        <div id="dedicated-hosts"></div>
        <button type="button" id="dedicated-refresh">Refresh Servers</button>
    </details>
</div>
