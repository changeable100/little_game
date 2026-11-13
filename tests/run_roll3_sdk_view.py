"""Compatibility entry for the current native SDK 3D view fixtures."""
from pathlib import Path
import runpy,sys
sys.argv=[sys.argv[0],'--roll3-view']
runpy.run_path(str(Path(__file__).resolve().parent.parent/'tools/run_game_sdk_ui.py'),run_name='__main__')
