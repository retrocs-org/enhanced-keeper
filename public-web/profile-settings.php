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
    <details>
        <summary>Change password</summary>
        <form method="post" id="profile-password-form">
            <label>Current password<input name="current_password" type="password" required maxlength="128" autocomplete="current-password"></label>
            <label>New password<input name="password" type="password" required minlength="12" maxlength="128" autocomplete="new-password"></label>
            <button>Change password</button>
        </form>
    </details>
    <details class="profile-delete">
        <summary>Permanently delete account</summary>
        <p>This removes your cloud profile, friendships, and uploaded replays. Local game saves stay on your device.</p>
        <form method="post" id="profile-delete-form">
            <label>Current password<input name="current_password" type="password" required maxlength="128" autocomplete="current-password"></label>
            <label>Type DELETE to confirm<input name="confirmation" required pattern="DELETE" autocomplete="off"></label>
            <button>Delete my account permanently</button>
        </form>
    </details>
</div>
