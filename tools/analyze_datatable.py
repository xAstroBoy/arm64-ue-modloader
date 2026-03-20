"""Analyze DebugMenuOptions DataTable JSON to map all rows by menu page."""
import json
import sys
import os

def main():
    json_path = os.path.join(os.path.dirname(__file__), '..', 'PAKS_extracted', 'json_dumps', 'DebugMenuOptions.json')
    with open(json_path) as f:
        data = json.load(f)
    
    rows = data['Exports'][0]['Table']['Data']
    print(f"Total DataTable rows: {len(rows)}")
    print()
    
    # Catalog all rows by menu type
    menus = {}
    for item in rows:
        if item.get('StructType') != 'DebugMenuOption':
            print(f"  NON-ROW: {item.get('Name')} ({item.get('$type', '')})")
            continue
        
        name = item['Name']
        fields = {f['Name'].split('_')[0]: f for f in item['Value']}
        menu_val = fields.get('Menu', {}).get('EnumValue', '?')
        opt_name = fields.get('OptionName', {}).get('Value', '?')
        opt_type = fields.get('OptionType', {}).get('EnumValue', '?')
        menu_link = fields.get('MenuLink', {}).get('EnumValue', '?')
        console_cmd = fields.get('ConsoleCommand', {}).get('Value', None)
        opt_list_val = fields.get('OptionList', {}).get('Value', [])
        opt_list_len = len(opt_list_val)
        opt_list_items = [v.get('Value', '?') for v in opt_list_val] if opt_list_len > 0 else []
        
        menu_num = menu_val.split('::')[1] if '::' in menu_val else menu_val
        type_num = opt_type.split('::')[1] if '::' in opt_type else opt_type
        link_num = menu_link.split('::')[1] if '::' in menu_link else menu_link
        
        if menu_num not in menus:
            menus[menu_num] = []
        menus[menu_num].append({
            'row_name': name,
            'display': opt_name,
            'type': type_num,
            'link': link_num,
            'list_len': opt_list_len,
            'list_items': opt_list_items,
            'cmd': console_cmd,
        })
    
    # Print by menu
    for menu_key in sorted(menus.keys()):
        entries = menus[menu_key]
        print(f"=== Menu {menu_key} ({len(entries)} options) ===")
        for e in entries:
            extras = []
            if e['list_len'] > 0:
                extras.append(f"list={e['list_len']}:{e['list_items']}")
            if e['cmd']:
                extras.append(f"cmd={e['cmd']}")
            if e['link'] != 'NewEnumerator5':
                extras.append(f"link->{e['link']}")
            extra_str = f"  [{'; '.join(extras)}]" if extras else ""
            print(f"  {e['row_name']}: \"{e['display']}\" type={e['type']}{extra_str}")
        print()
    
    # Also print enum value mapping summary
    print("=== DebugOptionType values seen ===")
    all_types = set()
    for entries in menus.values():
        for e in entries:
            all_types.add(e['type'])
    for t in sorted(all_types):
        print(f"  {t}")

if __name__ == '__main__':
    main()
