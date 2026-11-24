#include "../../src/game_saves.h"
#import <Cocoa/Cocoa.h>
#import <CommonCrypto/CommonDigest.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;

static NSFileManager *FM(void) { return NSFileManager.defaultManager; }
static NSString *BasePrefs(void) {
    NSDictionary *env=NSProcessInfo.processInfo.environment;
    NSString *override=env[@"KFX_BASE_USER_DATA_DIR"]?:env[@"KFX_USER_DATA_DIR"];
    return override.length?override:[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/keeperfx/keeperfx"];
}
static NSString *ProfileID(void) {
    NSString *value=NSProcessInfo.processInfo.environment[@"KFX_CONTENT_PROFILE"];
    if(!value)value=[NSString stringWithContentsOfFile:[BasePrefs() stringByAppendingPathComponent:@"content-profile.cfg"] encoding:NSUTF8StringEncoding error:nil];
    value=[value stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    NSCharacterSet *invalid=[[NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"] invertedSet];
    if(!value.length||value.length>32||[value rangeOfCharacterFromSet:invalid].location!=NSNotFound)return @"default";
    if(![value isEqual:@"default"]){NSString *parent=[BasePrefs() stringByAppendingPathComponent:@"ContentProfiles"],*folder=[parent stringByAppendingPathComponent:value];
        for(NSString *path in @[parent,folder])if([[[FM() attributesOfItemAtPath:path error:nil] objectForKey:NSFileType] isEqual:NSFileTypeSymbolicLink])return @"default";}
    return value;
}
static NSString *Prefs(void) { NSString *name=ProfileID();return [name isEqual:@"default"]?BasePrefs():[[BasePrefs() stringByAppendingPathComponent:@"ContentProfiles"] stringByAppendingPathComponent:name]; }
static NSString *Runtime(void) { return [Prefs() stringByAppendingPathComponent:@"GameData"]; }
static NSString *Owned(void) { return [BasePrefs() stringByAppendingPathComponent:@"OriginalData"]; }
static NSString *Resources(void) {
    return NSProcessInfo.processInfo.environment[@"KFX_RESOURCES_DIR"] ?: NSBundle.mainBundle.resourcePath;
}
static NSArray<NSString *> *Required(void) {
    return @[@"data/bluepal.dat", @"data/bluepall.dat", @"data/dogpal.pal", @"data/hitpall.dat", @"data/lightng.pal", @"data/redpal.col", @"data/redpall.dat", @"data/slab0-0.dat", @"data/slab0-1.dat", @"data/vampal.pal", @"data/whitepal.col", @"sound/atmos1.sbk", @"sound/atmos2.sbk", @"sound/bullfrog.sbk"];
}
// Walk each component to support mounted CDs with uppercase directory names.
static NSString *Find(NSString *base, NSString *relative) {
    NSString *current = base;
    for (NSString *part in relative.pathComponents) {
        NSString *found = nil;
        for (NSString *entry in [FM() contentsOfDirectoryAtPath:current error:nil]) {
            if ([entry caseInsensitiveCompare:part] == NSOrderedSame) { found = entry; break; }
        }
        if (!found) return nil;
        current = [current stringByAppendingPathComponent:found];
    }
    return current;
}
static BOOL ValidOriginal(NSString *base) {
    for (NSString *relative in Required()) {
        NSString *path = Find(base, relative);
        NSDictionary *attributes = path ? [FM() attributesOfItemAtPath:path error:nil] : nil;
        if (![attributes[NSFileType] isEqual:NSFileTypeRegular] || [attributes[NSFileSize] unsignedLongLongValue] == 0) return NO;
    }
    return YES;
}
static BOOL Directory(NSString *path, NSError **error) {
    if([[[FM() attributesOfItemAtPath:path error:nil] objectForKey:NSFileType] isEqual:NSFileTypeSymbolicLink]){if(error)*error=[NSError errorWithDomain:@"KeeperFX" code:4 userInfo:@{NSLocalizedDescriptionKey:@"A profile directory is linked. Use a regular directory to keep saves and content isolated."}];return NO;}
    return [FM() createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:error];
}
static BOOL CopyFile(NSString *source, NSString *destination, NSError **error) {
    if (!Directory(destination.stringByDeletingLastPathComponent, error)) return NO;
    NSString *temporary = [destination stringByAppendingFormat:@".%@.tmp", NSUUID.UUID.UUIDString];
    if (![FM() copyItemAtPath:source toPath:temporary error:error]) return NO;
    if ([FM() fileExistsAtPath:destination]) {
        if (![FM() removeItemAtPath:destination error:error]) { [FM() removeItemAtPath:temporary error:nil]; return NO; }
    }
    return [FM() moveItemAtPath:temporary toPath:destination error:error];
}
static BOOL Import(NSString *selection, NSError **error) {
    NSString *source = selection;
    if (!ValidOriginal(source)) source = Find(selection, @"keeper");
    if (!source || !ValidOriginal(source)) {
        if (error) *error = [NSError errorWithDomain:@"KeeperFX" code:1 userInfo:@{NSLocalizedDescriptionKey:@"Choose the KEEPER folder from your original CD or installed Dungeon Keeper game. All 14 required data and sound files must be present."}];
        return NO;
    }
    // Validate everything before changing the existing import.
    NSString *stage = [Prefs() stringByAppendingPathComponent:[@"Import-" stringByAppendingString:NSUUID.UUID.UUIDString]];
    for (NSString *relative in Required()) {
        if (!CopyFile(Find(source, relative), [stage stringByAppendingPathComponent:relative], error)) { [FM() removeItemAtPath:stage error:nil]; return NO; }
    }
    NSString *movies = Find(source, @"ldata");
    for (NSString *file in [FM() contentsOfDirectoryAtPath:movies ?: @"" error:nil]) {
        if ([file.pathExtension.lowercaseString isEqual:@"smk"] &&
            !CopyFile([movies stringByAppendingPathComponent:file], [stage stringByAppendingPathComponent:[@"ldata" stringByAppendingPathComponent:file.lowercaseString]], error)) {
            [FM() removeItemAtPath:stage error:nil]; return NO;
        }
    }
    NSString *backup = [Owned() stringByAppendingString:@".previous"];
    [FM() removeItemAtPath:backup error:nil];
    BOOL previous = [FM() fileExistsAtPath:Owned()];
    if (previous && ![FM() moveItemAtPath:Owned() toPath:backup error:error]) return NO;
    if (![FM() moveItemAtPath:stage toPath:Owned() error:error]) {
        if (previous) [FM() moveItemAtPath:backup toPath:Owned() error:nil];
        return NO;
    }
    [FM() removeItemAtPath:backup error:nil];
    // Force the next preparation to overlay the new import.
    [FM() removeItemAtPath:[Runtime() stringByAppendingPathComponent:@".asset-version"] error:nil];
    return YES;
}
static NSString *AssetVersion(void) {
    return [NSString stringWithContentsOfFile:[Resources() stringByAppendingPathComponent:@"asset-version.txt"] encoding:NSUTF8StringEncoding error:nil] ?: @"dev";
}
static BOOL ReservedRuntimePath(NSString *name) {
    NSString *first=name.pathComponents.firstObject;
    return [@[@"save",@"scrshots",@"keeperfx",@".asset-version",@".profile-overlay-files.json"] containsObject:first];
}
static NSString *SafeRuntimePath(NSString *relative) {
    if(relative.isAbsolutePath||![relative.stringByStandardizingPath isEqual:relative]||[relative.pathComponents containsObject:@".."]||[relative.pathComponents containsObject:@"."]||ReservedRuntimePath(relative))return nil;
    NSString *path=Runtime();for(NSString *part in relative.pathComponents){path=[path stringByAppendingPathComponent:part];if([[[FM() attributesOfItemAtPath:path error:nil] objectForKey:NSFileType] isEqual:NSFileTypeSymbolicLink])return nil;}return path;
}
static NSArray<NSString *> *OverlayFiles(void) {
    NSString *root=[Prefs() stringByAppendingPathComponent:@"EnhancedData"];NSMutableArray *files=[NSMutableArray new];
    for(NSString *name in [FM() enumeratorAtPath:root]){
        if([name isEqual:@"keeperfx.cfg"]||ReservedRuntimePath(name))continue;
        if([[[FM() attributesOfItemAtPath:[root stringByAppendingPathComponent:name] error:nil] objectForKey:NSFileType] isEqual:NSFileTypeRegular])[files addObject:name];
    }
    return [files sortedArrayUsingSelector:@selector(compare:)];
}
static NSString *OverlayVersion(NSArray<NSString *> *files) {
    CC_SHA256_CTX context;CC_SHA256_Init(&context);NSString *root=[Prefs() stringByAppendingPathComponent:@"EnhancedData"];
    for(NSString *name in files){NSDictionary *attrs=[FM() attributesOfItemAtPath:[root stringByAppendingPathComponent:name] error:nil];NSString *line=[NSString stringWithFormat:@"%@:%@:%.9f\n",name,attrs[NSFileSize],[attrs[NSFileModificationDate] timeIntervalSince1970]];NSData *data=[line dataUsingEncoding:NSUTF8StringEncoding];CC_SHA256_Update(&context,data.bytes,(CC_LONG)data.length);}
    unsigned char bytes[CC_SHA256_DIGEST_LENGTH];CC_SHA256_Final(bytes,&context);NSMutableString *result=[NSMutableString new];for(int i=0;i<CC_SHA256_DIGEST_LENGTH;i++)[result appendFormat:@"%02x",bytes[i]];return result;
}
// Reversible profile reset and exports deliberately exclude imported game data.
static BOOL ResetRequested(NSError **error) {
    NSString *marker=[Prefs() stringByAppendingPathComponent:@"reset-profile-next.cfg"];
    if(![FM() fileExistsAtPath:marker])return YES;
    NSString *folder=[[BasePrefs() stringByAppendingPathComponent:@"ProfileBackups"] stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@",ProfileID(),NSUUID.UUID.UUIDString]];
    if(!Directory([BasePrefs() stringByAppendingPathComponent:@"ProfileBackups"],error)||!Directory(folder,error))return NO;
    NSMutableArray *moved=[NSMutableArray new];
    for(NSString *name in @[@"GameData",@"save",@"scrshots",@"EnhancedData"]){
        NSString *from=[Prefs() stringByAppendingPathComponent:name],*to=[folder stringByAppendingPathComponent:name];
        if(![FM() fileExistsAtPath:from])continue;
        if(![FM() moveItemAtPath:from toPath:to error:error]){
            for(NSString *previous in moved.reverseObjectEnumerator)[FM() moveItemAtPath:[folder stringByAppendingPathComponent:previous] toPath:[Prefs() stringByAppendingPathComponent:previous] error:nil];return NO;
        }[moved addObject:name];
    }
    [FM() removeItemAtPath:marker error:nil];return YES;
}
static BOOL ExportPersonal(NSError **error) {
    NSString *exports=[BasePrefs() stringByAppendingPathComponent:@"Exports"];
    if(!Directory(exports,error))return NO;
    NSString *stage=[exports stringByAppendingPathComponent:[@"Personal-" stringByAppendingString:NSUUID.UUID.UUIDString]];
    if(!Directory(stage,error))return NO;
    BOOL ok=YES;NSMutableArray *profiles=[NSMutableArray arrayWithObject:@[@"",BasePrefs()]];
    NSString *profilesRoot=[BasePrefs() stringByAppendingPathComponent:@"ContentProfiles"];
    if(![[[FM() attributesOfItemAtPath:profilesRoot error:nil] objectForKey:NSFileType] isEqual:NSFileTypeSymbolicLink])for(NSString *name in [FM() contentsOfDirectoryAtPath:profilesRoot error:nil]){
        NSString *folder=[profilesRoot stringByAppendingPathComponent:name];if(![[[FM() attributesOfItemAtPath:folder error:nil] objectForKey:NSFileType] isEqual:NSFileTypeDirectory])continue;
        [profiles addObject:@[[@"ContentProfiles" stringByAppendingPathComponent:name],folder]];
    }
    for(NSArray *profile in profiles){NSString *prefix=profile[0],*folder=profile[1];
        for(NSString *root in @[@"save",@"scrshots",@"EnhancedData"]){
            NSString *base=[folder stringByAppendingPathComponent:root];if([[[FM() attributesOfItemAtPath:base error:nil] objectForKey:NSFileType] isEqual:NSFileTypeSymbolicLink])continue;
            for(NSString *name in [FM() enumeratorAtPath:base]){
                NSString *leaf=name.lastPathComponent.lowercaseString;
                if([leaf containsString:@"session"]||[leaf containsString:@"token"]||[leaf containsString:@"password"]||[leaf containsString:@"credential"])continue;
                NSString *source=[base stringByAppendingPathComponent:name];
                if(![[[FM() attributesOfItemAtPath:source error:nil] objectForKey:NSFileType] isEqual:NSFileTypeRegular])continue;
                NSString *relative=[[prefix stringByAppendingPathComponent:root] stringByAppendingPathComponent:name];
                if(!CopyFile(source,[stage stringByAppendingPathComponent:relative],error)){ok=NO;break;}
            }if(!ok)break;
        }if(!ok)break;
        NSString *config=[[folder stringByAppendingPathComponent:@"GameData"] stringByAppendingPathComponent:@"keeperfx.cfg"];
        if([[[FM() attributesOfItemAtPath:config error:nil] objectForKey:NSFileType] isEqual:NSFileTypeRegular]&&[[[FM() attributesOfItemAtPath:config.stringByDeletingLastPathComponent error:nil] objectForKey:NSFileType] isEqual:NSFileTypeDirectory]){ok=CopyFile(config,[[stage stringByAppendingPathComponent:prefix] stringByAppendingPathComponent:@"keeperfx.cfg"],error);if(!ok)break;}
    }
    NSString *archive=[stage stringByAppendingString:@".zip"];
    if(ok){NSTask *zip=[NSTask new];zip.executableURL=[NSURL fileURLWithPath:@"/usr/bin/ditto"];zip.arguments=@[@"-c",@"-k",@"--keepParent",stage,archive];ok=[zip launchAndReturnError:error];if(ok){[zip waitUntilExit];ok=zip.terminationStatus==0;}}
    [FM() removeItemAtPath:stage error:nil];
    if(ok)printf("%s\n",archive.fileSystemRepresentation);else[FM() removeItemAtPath:archive error:nil];return ok;
}
static BOOL RecoverDisplay(NSError **error) {
    NSString *marker=[Prefs() stringByAppendingPathComponent:@"display-startup.pending"];
    if(![FM() fileExistsAtPath:marker])return YES;
    NSString *file=[Runtime() stringByAppendingPathComponent:@"keeperfx.cfg"];
    NSMutableString *config=[[NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:error] mutableCopy];if(!config)return NO;
    NSString *backup=[file stringByAppendingFormat:@".before-display-recovery-%@",NSUUID.UUID.UUIDString];if(!CopyFile(file,backup,error))return NO;
    NSRegularExpression *regex=[NSRegularExpression regularExpressionWithPattern:@"(?im)^(FRONTEND_RES|INGAME_RES)\\s*=.*$" options:0 error:nil];
    [regex replaceMatchesInString:config options:0 range:NSMakeRange(0,config.length) withTemplate:@"$1=800x600w32 800x600w32 800x600w32"];
    if(![config writeToFile:file atomically:YES encoding:NSUTF8StringEncoding error:error])return NO;
    NSString *options=[[Prefs() stringByAppendingPathComponent:@"save"] stringByAppendingPathComponent:@"macos-options.cfg"];
    NSMutableString *values=[[NSString stringWithContentsOfFile:options encoding:NSUTF8StringEncoding error:nil] mutableCopy];
    if(values){if(!CopyFile(options,[backup stringByAppendingString:@".options"],error))return NO;for(NSString *key in @[@"display_mode",@"resolution",@"window_cursor_lock"]){NSRegularExpression *rule=[NSRegularExpression regularExpressionWithPattern:[NSString stringWithFormat:@"(?m)^%@=.*$",key] options:0 error:nil];[rule replaceMatchesInString:values options:0 range:NSMakeRange(0,values.length) withTemplate:[key stringByAppendingString:@"=0"]];}if(![values writeToFile:options atomically:YES encoding:NSUTF8StringEncoding error:error])return NO;}
    [@"A previous display startup did not finish. Safe windowed mode was restored; your old configuration was retained." writeToFile:[Prefs() stringByAppendingPathComponent:@"display-recovered.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    [FM() removeItemAtPath:marker error:nil];return YES;
}
static BOOL Prepare(NSError **error) {
    if(!ResetRequested(error))return NO;
    for (NSString *sub in @[@"", @"save", @"scrshots", @"GameData"])
        if (!Directory([Prefs() stringByAppendingPathComponent:sub], error)) return NO;
    NSArray *overlay=OverlayFiles();NSString *desired=[NSString stringWithFormat:@"%@\nOverlay:%@\nOriginal:%.9f",AssetVersion(),OverlayVersion(overlay),[[[FM() attributesOfItemAtPath:Owned() error:nil] objectForKey:NSFileModificationDate] timeIntervalSince1970]];
    NSString *stamp = [Runtime() stringByAppendingPathComponent:@".asset-version"];
    NSString *old = [NSString stringWithContentsOfFile:stamp encoding:NSUTF8StringEncoding error:nil];
    if (![old isEqual:desired]) {
        NSString *manifest=[Runtime() stringByAppendingPathComponent:@".profile-overlay-files.json"];
        NSData *record=[NSData dataWithContentsOfFile:manifest];id previous=record?[NSJSONSerialization JSONObjectWithData:record options:0 error:nil]:nil;
        if([previous isKindOfClass:NSArray.class])for(id item in previous){
            if(![item isKindOfClass:NSString.class]||[item isEqual:@"keeperfx.cfg"])continue;
            NSString *destination=SafeRuntimePath(item);if(destination&&[[[FM() attributesOfItemAtPath:destination error:nil] objectForKey:NSFileType] isEqual:NSFileTypeRegular])[FM() removeItemAtPath:destination error:nil];
        }
        NSString *assets = [Resources() stringByAppendingPathComponent:@"game"];
        for (NSString *base in @[assets, Owned(), [Prefs() stringByAppendingPathComponent:@"EnhancedData"]]) {
            NSDirectoryEnumerator *files = [FM() enumeratorAtPath:base];
            for (NSString *relative in files) {
                if (ReservedRuntimePath(relative)) continue;
                NSString *source = [base stringByAppendingPathComponent:relative];
                NSDictionary *attrs = [FM() attributesOfItemAtPath:source error:error];
                if (!attrs) return NO;
                if ([attrs[NSFileType] isEqual:NSFileTypeDirectory]) continue;
                if (![attrs[NSFileType] isEqual:NSFileTypeRegular]) continue;
                NSString *dest = SafeRuntimePath(relative);
                if(!dest){if(error)*error=[NSError errorWithDomain:@"KeeperFX" code:3 userInfo:@{NSLocalizedDescriptionKey:@"The runtime contains a linked or invalid content path. Remove links from GameData before preparing this profile."}];return NO;}
                if ([relative isEqual:@"keeperfx.cfg"] && [FM() fileExistsAtPath:dest]) continue;
                if (!CopyFile(source, dest, error)) return NO;
            }
        }
        if (!ValidOriginal(Runtime())) {
            if (error) *error = [NSError errorWithDomain:@"KeeperFX" code:2 userInfo:@{NSLocalizedDescriptionKey:@"Original Dungeon Keeper files have not been imported. Choose Import Original Game first."}];
            return NO;
        }
        NSData *recorded=[NSJSONSerialization dataWithJSONObject:overlay options:0 error:error];
        if(!recorded||![recorded writeToFile:manifest options:NSDataWritingAtomic error:error])return NO;
        if (![desired writeToFile:stamp atomically:YES encoding:NSUTF8StringEncoding error:error]) return NO;
    }
    return RecoverDisplay(error);
}
static void SetConfig(NSMutableString *text, NSString *key, NSString *value) {
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:[NSString stringWithFormat:@"(?m)^%@\\s*=.*$", key] options:NSRegularExpressionCaseInsensitive error:nil];
    NSString *line = [NSString stringWithFormat:@"%@=%@", key, value];
    NSTextCheckingResult *match = [regex firstMatchInString:text options:0 range:NSMakeRange(0,text.length)];
    if (match) [text replaceCharactersInRange:match.range withString:line];
    else [text appendFormat:@"\n%@\n",line];
}
static NSArray *ControllerActions(void){
    NSData *data=[NSData dataWithContentsOfFile:[Resources() stringByAppendingPathComponent:@"controller-actions.json"]];
    return data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:@[];
}
static BOOL ApplyControllerBindings(NSMutableString *settings, NSDictionary *bindings, NSError **error){
    (void)error;
    NSRange section=[settings rangeOfString:@"[keys]"];
    if(section.location==NSNotFound){[settings appendString:@"\n[keys]\n"];section=[settings rangeOfString:@"[keys]"];}
    for(NSString *action in [[bindings allKeys] sortedArrayUsingSelector:@selector(compare:)]){
        NSString *value=bindings[action];
        NSString *pattern=[NSString stringWithFormat:@"(?m)^\\s*%@\\s*=\\s*\\{[^\\n]*$",[NSRegularExpression escapedPatternForString:action]];
        NSRegularExpression *lineRx=[NSRegularExpression regularExpressionWithPattern:pattern options:0 error:nil];
        NSTextCheckingResult *line=[lineRx firstMatchInString:settings options:0 range:NSMakeRange(0,settings.length)];
        if(line){
            NSMutableString *text=[[settings substringWithRange:line.range] mutableCopy];
            NSRegularExpression *rx=[NSRegularExpression regularExpressionWithPattern:@"controller_buttons\\s*=\\s*\"([^\"]*)\"" options:0 error:nil];
            NSTextCheckingResult *match=[rx firstMatchInString:text options:0 range:NSMakeRange(0,text.length)];
            if(match)[text replaceCharactersInRange:[match rangeAtIndex:1] withString:value];
            else {NSRange end=[text rangeOfString:@"}" options:NSBackwardsSearch];if(end.location!=NSNotFound)[text insertString:[NSString stringWithFormat:@", controller_buttons = \"%@\" ",value] atIndex:end.location];}
            [settings replaceCharactersInRange:line.range withString:text];
        } else {
            section=[settings rangeOfString:@"[keys]"];
            [settings insertString:[NSString stringWithFormat:@"\n%@ = { controller_buttons = \"%@\" }",action,value] atIndex:NSMaxRange(section)];
        }
    }
    return YES;
}

static pid_t Launch(NSArray<NSString *> *extra, NSNumber *slot, NSError **error) {
    [@"startup" writeToFile:[Prefs() stringByAppendingPathComponent:@"display-startup.pending"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    NSString *engine = [Resources() stringByAppendingPathComponent:@"game/keeperfx"];
    // The engine derives its runtime root from argv[0]; keep the signed executable
    // in the app while directing ALL legacy relative reads/writes to user data.
    NSArray *args = [[@[[Runtime() stringByAppendingPathComponent:@"keeperfx"]] arrayByAddingObjectsFromArray:extra] copy];
    char **argv = calloc(args.count+1,sizeof(char *));
    if (!argv) return -1;
    for (NSUInteger i=0;i<args.count;i++) argv[i] = strdup([args[i] UTF8String]);
    pid_t pid = -1;
    NSMutableDictionary *environment=[NSProcessInfo.processInfo.environment mutableCopy];
    environment[@"KFX_APP_RESOURCES"]=Resources();
    environment[@"KFX_BASE_USER_DATA_DIR"]=BasePrefs();
    environment[@"KFX_USER_DATA_DIR"]=Prefs();
    environment[@"KFX_CONTENT_PROFILE"]=ProfileID();
    if(slot)environment[@"KFX_LOAD_SAVE_SLOT"]=slot.stringValue;else[environment removeObjectForKey:@"KFX_LOAD_SAVE_SLOT"];
    char **envp=calloc(environment.count+1,sizeof(char *));NSUInteger envIndex=0;
    if(!envp){for(NSUInteger i=0;i<args.count;i++)free(argv[i]);free(argv);return -1;}
    for(NSString *key in environment)envp[envIndex++]=strdup([[NSString stringWithFormat:@"%@=%@",key,environment[key]] UTF8String]);
    int result = posix_spawn(&pid,engine.fileSystemRepresentation,NULL,NULL,argv,envp);
    for(NSUInteger i=0;i<envIndex;i++)free(envp[i]);free(envp);
    for (NSUInteger i=0;i<args.count;i++) free(argv[i]);
    free(argv);
    if (result) {
        if (error) *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:result userInfo:nil];
        return -1;
    }
    return pid;
}
static NSDictionary *LegacySaveMetadata(NSString *path) {
    FILE *file=fopen(path.fileSystemRepresentation,"rb");if(!file)return @{};
    struct FileChunkHeader header;struct CatalogueEntry entry;
    BOOL valid=fread(&header,sizeof(header),1,file)==1 && header.id==SGC_InfoBlock && header.len==sizeof(entry) && fread(&entry,sizeof(entry),1,file)==1;fclose(file);
    if(!valid)return @{};
    entry.textname[sizeof(entry.textname)-1]=0;entry.campaign_name[sizeof(entry.campaign_name)-1]=0;
    NSString *title=[NSString stringWithUTF8String:entry.textname],*campaign=[NSString stringWithUTF8String:entry.campaign_name];
    return @{@"title":title?:@"Saved game",@"campaign":campaign?:@"Unknown campaign",@"level":@(entry.level_num),@"automatic":@([title hasPrefix:@"Automatic save "])};
}
@interface Launcher : NSObject <NSApplicationDelegate>
@property NSWindow *window;
@property NSPopUpButton *display;
@property NSPopUpButton *resolution;
@property NSPopUpButton *renderer;
@property NSSlider *volume;
@property NSButton *intro;
@property NSTextField *status;
@property NSButton *play;
@property BOOL working;
@property NSNumber *pendingLoadSlot;
@property NSPopUpButton *bindingAction;
@property NSComboBox *bindingValue;
@property pid_t gamePID;
@property NSPopUpButton *guideModel;
@property NSImageView *guideImage;
@property NSPopUpButton *saveChoice;
@property NSImageView *saveImage;
@property NSTextField *saveDetails;
@property NSArray *saveEntries;
@end
@implementation Launcher
- (void)alert:(NSString *)message { NSAlert *a=[NSAlert new];a.messageText=@"KeeperFX-E";a.informativeText=message;[a runModal]; }
- (NSButton *)button:(NSString *)title action:(SEL)action {
    NSButton *b=[NSButton buttonWithTitle:title target:self action:action];b.bezelStyle=NSBezelStyleRounded;return b;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    NSMenu *menu=[NSMenu new];NSMenuItem *app=[NSMenuItem new];[menu addItem:app];NSMenu *sub=[NSMenu new];
    [sub addItemWithTitle:@"Quit KeeperFX-E" action:@selector(terminate:) keyEquivalent:@"q"];app.submenu=sub;NSApp.mainMenu=menu;
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,560,600) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"KeeperFX-E";[self.window center];
    NSStackView *stack=[NSStackView new];stack.orientation=NSUserInterfaceLayoutOrientationVertical;stack.alignment=NSLayoutAttributeLeading;stack.spacing=14;stack.translatesAutoresizingMaskIntoConstraints=NO;
    [self.window.contentView addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[[stack.leadingAnchor constraintEqualToAnchor:self.window.contentView.leadingAnchor constant:28],[stack.trailingAnchor constraintEqualToAnchor:self.window.contentView.trailingAnchor constant:-28],[stack.topAnchor constraintEqualToAnchor:self.window.contentView.topAnchor constant:26]]];
    NSTextField *title=[NSTextField labelWithString:@"Dungeon Keeper • KeeperFX-E"];title.font=[NSFont boldSystemFontOfSize:24];[stack addArrangedSubview:title];
    NSTextField *desc=[NSTextField wrappingLabelWithString:@"Your native Mac dungeon. Import your original game once, choose your settings, and play."];[stack addArrangedSubview:desc];
    self.display=[NSPopUpButton new];[self.display addItemsWithTitles:@[@"Windowed",@"Borderless fullscreen",@"Exclusive fullscreen"]];
    self.resolution=[NSPopUpButton new];[self.resolution addItemsWithTitles:@[@"1280×800",@"1920×1080",@"2560×1440",@"640×480"]];
    self.renderer=[NSPopUpButton new];[self.renderer addItemsWithTitles:@[@"Software / Metal (recommended)",@"OpenGL (experimental)"]];
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    [self.display selectItemAtIndex:[defaults objectForKey:@"display"]?[defaults integerForKey:@"display"]:1];
    [self.resolution selectItemAtIndex:[defaults integerForKey:@"resolution"]];[self.renderer selectItemAtIndex:[defaults integerForKey:@"renderer"]];
    for (NSArray *row in @[@[@"Display",self.display],@[@"Window size",self.resolution],@[@"Renderer",self.renderer]]) {
        NSStackView *line=[NSStackView stackViewWithViews:@[[NSTextField labelWithString:row[0]],row[1]]];line.spacing=18;[stack addArrangedSubview:line];
    }
    self.volume=[NSSlider sliderWithValue:[defaults objectForKey:@"music"]?[defaults doubleForKey:@"music"]:90 minValue:0 maxValue:127 target:nil action:nil];[self.volume.widthAnchor constraintEqualToConstant:280].active=YES;
    [stack addArrangedSubview:[NSStackView stackViewWithViews:@[[NSTextField labelWithString:@"Music volume"],self.volume]]];
    self.intro=[NSButton checkboxWithTitle:@"Play opening cinematics" target:nil action:nil];self.intro.state=[defaults objectForKey:@"intro"]?[defaults boolForKey:@"intro"]:NSControlStateValueOn;[stack addArrangedSubview:self.intro];
    [stack addArrangedSubview:[NSStackView stackViewWithViews:@[[self button:@"Import Original Game…" action:@selector(importGame:)],[self button:@"Options…" action:@selector(options:)]]]];
    [stack addArrangedSubview:[NSStackView stackViewWithViews:@[[self button:@"Test Online Connection" action:@selector(testConnection:)],[self button:@"Controllers…" action:@selector(controllers:)],[self button:@"Remap…" action:@selector(remapControls:)]]]];
    [stack addArrangedSubview:[NSStackView stackViewWithViews:@[[self button:@"Browse Saves" action:@selector(browseSaves:)],[self button:@"Open Logs" action:@selector(openLogs:)],[self button:@"Open Latest Save" action:@selector(latestSave:)]]]];
    self.status=[NSTextField wrappingLabelWithString:ValidOriginal(Owned())?@"Original game files are ready.":@"First run: import the KEEPER folder from your CD or existing Dungeon Keeper installation."];[stack addArrangedSubview:self.status];
    self.play=[self button:@"Play KeeperFX-E" action:@selector(playGame:)];self.play.keyEquivalent=@"\r";[stack addArrangedSubview:self.play];
    [self.window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { (void)sender;return self.gamePID<=0; }
- (void)openSaves:(id)sender { (void)sender;NSString *p=[Prefs() stringByAppendingPathComponent:@"save"];Directory(p,nil);[NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:p]]; }
- (void)openLogs:(id)sender { (void)sender;Directory(Prefs(),nil);[NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:Prefs()]]; }
- (void)importGame:(id)sender {
    (void)sender;if(self.working||self.gamePID>0)return;
    NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=YES;panel.canChooseFiles=NO;panel.message=@"Choose the KEEPER folder or mounted Dungeon Keeper disc.";
    if([panel runModal]!=NSModalResponseOK)return;
    self.working=YES;self.status.stringValue=@"Importing original game files…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{NSError *error=nil;BOOL ok=Import(panel.URL.path,&error);
        dispatch_async(dispatch_get_main_queue(),^{self.working=NO;self.status.stringValue=ok?@"Original game files imported. You can now play without the CD.":error.localizedDescription;});});
}
- (void)testConnection:(id)sender {
    (void)sender;if(self.working)return;self.working=YES;self.status.stringValue=@"Testing the secure lobby connection (up to 8 seconds)…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSTask *task=[NSTask new];task.executableURL=[NSURL fileURLWithPath:[Resources() stringByAppendingPathComponent:@"connection-check"]];
        NSPipe *pipe=[NSPipe pipe];task.standardOutput=pipe;task.standardError=pipe;NSError *error=nil;
        BOOL started=[task launchAndReturnError:&error];if(started)[task waitUntilExit];
        NSString *output=started?[[NSString alloc] initWithData:pipe.fileHandleForReading.readDataToEndOfFile encoding:NSUTF8StringEncoding]:error.localizedDescription;
        dispatch_async(dispatch_get_main_queue(),^{
            self.working=NO;self.status.stringValue=@"Online diagnostics complete.";
            NSDictionary *build=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[Resources() stringByAppendingPathComponent:@"build-info.json"]] options:0 error:nil]?:@{};
            NSAlert *a=[NSAlert new];a.messageText=@"Multiplayer diagnostics";
            a.informativeText=[NSString stringWithFormat:@"Game version: %@\nMac app build: %@\nEngine: %@\n\n%@\nBoth players should use matching game versions, app builds and mods. A reachable lobby server does not prove that a peer can reach your UDP game port.",build[@"gameVersion"]?:@"Unknown",build[@"appBuild"]?:@"Unknown",[build[@"engineSHA"]?:@"unknown" substringToIndex:MIN(12,[build[@"engineSHA"]?:@"unknown" length])],output?:@"No diagnostic result."];
            [a addButtonWithTitle:@"Done"];[a addButtonWithTitle:@"Compare Version…"];
            if([a runModal]==NSAlertSecondButtonReturn){
                NSString *version=build[@"gameVersion"]?:@"Unknown";
                dispatch_async(dispatch_get_main_queue(),^{[self compareVersion:version];});
            }
        });
    });
}
- (void)controllers:(id)sender {
    (void)sender;if(self.working)return;self.working=YES;self.status.stringValue=@"Checking connected controllers…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSTask *task=[NSTask new];task.executableURL=[NSURL fileURLWithPath:[Resources() stringByAppendingPathComponent:@"controller-check"]];
        task.arguments=@[[Resources() stringByAppendingPathComponent:@"game/fxdata/gamecontrollerdb.txt"]];
        NSPipe *pipe=[NSPipe pipe];task.standardOutput=pipe;task.standardError=pipe;NSError *error=nil;
        BOOL started=[task launchAndReturnError:&error];if(started)[task waitUntilExit];
        NSString *output=started?[[NSString alloc] initWithData:pipe.fileHandleForReading.readDataToEndOfFile encoding:NSUTF8StringEncoding]:error.localizedDescription;
        dispatch_async(dispatch_get_main_queue(),^{
            self.working=NO;self.status.stringValue=output.length?output:@"Controller check complete.";
            NSAlert *a=[NSAlert new];a.messageText=@"Xbox & DualSense controllers";
            a.informativeText=[(output?:@"Controller check unavailable.") stringByAppendingString:@"\nDefault controls — Xbox / DualSense\n\nLeft stick: move camera\nRight stick: move pointer\nA / Cross: confirm in menus (B / Circle goes back)\nRT / R2: select, click or drag\nLT / L2: right-click, drop or slap\nD-pad: jump between interface buttons\nHold A / Cross + D-pad: rotate or zoom\nHold A / Cross: faster camera movement\nY / Triangle: jump to Dungeon Heart\nX / Square: jump to a fight\nLB/RB / L1/R1: previous/next possessed ability\nMenu / Options: open or close pause menu\n\nUSB and supported Bluetooth controllers can connect before or during play. Remapping and pointer speed are available from the launcher. Existing custom bindings are preserved unless changed here.\n\nFor Bluetooth pairing, use macOS System Settings > Bluetooth."];
            [a addButtonWithTitle:@"Done"];[a addButtonWithTitle:@"Remap Controls…"];[a addButtonWithTitle:@"Check Again"];
            NSModalResponse response=[a runModal];
            if(response==NSAlertSecondButtonReturn)dispatch_async(dispatch_get_main_queue(),^{[self remapControls:nil];});
            if(response==NSAlertThirdButtonReturn)dispatch_async(dispatch_get_main_queue(),^{[self controllers:nil];});
        });
    });
}
- (void)playGame:(id)sender {
    (void)sender;if(self.working||self.gamePID>0)return;
    if(!ValidOriginal(Owned())){[self importGame:nil];return;}
    self.working=YES;self.play.enabled=NO;self.status.stringValue=@"Preparing your game data…";
    NSInteger display=self.display.indexOfSelectedItem,res=self.resolution.indexOfSelectedItem,renderer=self.renderer.indexOfSelectedItem;
    double music=self.volume.doubleValue;BOOL intro=self.intro.state==NSControlStateValueOn;
    NSNumber *loadSlot=self.pendingLoadSlot;self.pendingLoadSlot=nil;
    NSUserDefaults *d=NSUserDefaults.standardUserDefaults;[d setInteger:display forKey:@"display"];[d setInteger:res forKey:@"resolution"];[d setInteger:renderer forKey:@"renderer"];[d setDouble:music forKey:@"music"];[d setBool:intro forKey:@"intro"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSError *error=nil;BOOL ok=Prepare(&error);
        if(ok){
            NSString *path=[Runtime() stringByAppendingPathComponent:@"keeperfx.cfg"];
            NSMutableString *cfg=[[NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&error] mutableCopy];
            NSArray *sizes=@[@"1280x800w32",@"1920x1080w32",@"2560x1440w32",@"640x480w32"];
            NSString *mode=display==0?sizes[res]:(display==1?@"DESKTOP":@"DESKTOP_FULL");
            if(cfg){SetConfig(cfg,@"FRONTEND_RES",[NSString stringWithFormat:@"640x480w32 %@ %@",mode,mode]);SetConfig(cfg,@"INGAME_RES",mode);SetConfig(cfg,@"RENDERER",renderer?@"OPENGL":@"SOFTWARE");ok=[cfg writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:&error];}else ok=NO;
            if(ok){
                NSString *settingsPath=[Prefs() stringByAppendingPathComponent:@"save/settings.toml"];
                NSMutableString *settings=[[NSString stringWithContentsOfFile:settingsPath encoding:NSUTF8StringEncoding error:nil] mutableCopy]?:[NSMutableString stringWithString:@"[audio]\n"];
                NSRegularExpression *rx=[NSRegularExpression regularExpressionWithPattern:@"(?m)^music_volume\\s*=.*$" options:0 error:nil];
                NSTextCheckingResult *m=[rx firstMatchInString:settings options:0 range:NSMakeRange(0,settings.length)];NSString *line=[NSString stringWithFormat:@"music_volume = %d",(int)music];
                if(m)[settings replaceCharactersInRange:m.range withString:line];
                else { NSRange a=[settings rangeOfString:@"[audio]"];if(a.location!=NSNotFound)[settings insertString:[@"\n" stringByAppendingString:line] atIndex:NSMaxRange(a)];else[settings appendFormat:@"\n[audio]\n%@\n",line]; }
                ApplyControllerBindings(settings,[NSUserDefaults.standardUserDefaults dictionaryForKey:@"controllerBindings"]?:@{},&error);
                ok=[settings writeToFile:settingsPath atomically:YES encoding:NSUTF8StringEncoding error:&error];
            }
        }
        pid_t child=ok?Launch(intro&&!loadSlot?@[]:@[@"-nointro"],loadSlot,&error):-1;
        dispatch_async(dispatch_get_main_queue(),^{self.working=NO;if(child<0){self.play.enabled=YES;self.status.stringValue=error.localizedDescription?:@"Unable to start the game.";}else{self.gamePID=child;self.status.stringValue=@"Game running. Quit the game to return to the launcher.";[self.window orderOut:nil];}});
        if(child>0){int status=0;while(waitpid(child,&status,0)<0 && errno==EINTR){}dispatch_async(dispatch_get_main_queue(),^{self.gamePID=0;self.play.enabled=YES;self.status.stringValue=WIFEXITED(status)&&WEXITSTATUS(status)==0?@"Ready to play.":@"The game stopped unexpectedly. Open Logs for details.";[self.window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];});}
    });
}
#include "LauncherOptions.inc"
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender;if(self.gamePID>0){[self alert:@"Quit KeeperFX-E from the in-game menu before closing the launcher."];return NSTerminateCancel;}return NSTerminateNow;
}
@end
int main(int argc,char **argv){@autoreleasepool{
    NSArray *args=NSProcessInfo.processInfo.arguments;
    if([args containsObject:@"--export-personal"]){NSError *e=nil;BOOL ok=ExportPersonal(&e);if(!ok)fprintf(stderr,"Export failed: %s\n",e.localizedDescription.UTF8String);return ok?0:1;}
    if(argc>2 && !strcmp(argv[1],"--import")){NSError *e=nil;BOOL ok=Import(@(argv[2]),&e);if(!ok)fprintf(stderr,"%s\n",e.localizedDescription.UTF8String);return ok?0:1;}
    if([args containsObject:@"--prepare-only"]||[args containsObject:@"--play-direct"]){NSError *e=nil;if(!Prepare(&e)){fprintf(stderr,"%s\n",e.localizedDescription.UTF8String);return 1;}if([args containsObject:@"--prepare-only"]){puts("User runtime ready; application bundle unchanged.");return 0;}pid_t p=Launch(@[@"-nointro",@"-log",@"launcher-smoke.log"],nil,&e);if(p<0)return 1;int status=0;waitpid(p,&status,0);if(WIFEXITED(status)&&WEXITSTATUS(status)==0)[FM() removeItemAtPath:[Prefs() stringByAppendingPathComponent:@"display-startup.pending"] error:nil];return WIFEXITED(status)?WEXITSTATUS(status):1;}
    if(![args containsObject:@"--launcher"]){
        NSApplication *setup=NSApplication.sharedApplication;setup.activationPolicy=NSApplicationActivationPolicyAccessory;
        NSError *error=nil;
        if(!ValidOriginal(Owned()) && !ValidOriginal(Runtime())){
            [setup activateIgnoringOtherApps:YES];NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=YES;panel.canChooseFiles=NO;panel.message=@"Select your original Dungeon Keeper KEEPER folder or mounted CD. This is needed only once.";
            if([panel runModal]!=NSModalResponseOK)return 0;
            if(!Import(panel.URL.path,&error)){NSAlert *alert=[NSAlert new];alert.messageText=@"KeeperFX-E import failed";alert.informativeText=error.localizedDescription;[alert runModal];return 1;}
        }
        if(!Prepare(&error)){NSAlert *alert=[NSAlert new];alert.messageText=@"KeeperFX-E could not prepare game data";alert.informativeText=error.localizedDescription;[alert runModal];return 1;}
        pid_t child=Launch(@[],nil,&error);if(child<0){NSAlert *alert=[NSAlert new];alert.messageText=@"KeeperFX-E could not start";alert.informativeText=error.localizedDescription?:@"Unable to start the game.";[alert runModal];return 1;}
        int status=0;while(waitpid(child,&status,0)<0 && errno==EINTR){}if(WIFEXITED(status)&&WEXITSTATUS(status)==0)[FM() removeItemAtPath:[Prefs() stringByAppendingPathComponent:@"display-startup.pending"] error:nil];return WIFEXITED(status)?WEXITSTATUS(status):1;
    }
    NSApplication *app=NSApplication.sharedApplication;app.activationPolicy=NSApplicationActivationPolicyRegular;Launcher *delegate=[Launcher new];app.delegate=delegate;[app run];
}return 0;}
