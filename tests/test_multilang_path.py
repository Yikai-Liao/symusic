import os
import pytest
from symusic import Score

@pytest.mark.parametrize(
    "filepath",
    [
        "tests/testcases/测试文件.midi",
        "tests/testcases/ملف_اختبار.midi",
        "tests/testcases/テストファイル.midi",
        "tests/testcases/테스트파일.midi",
        "tests/testcases/文件_#@!.midi",
    ],
)
def test_multilang_file_read(filepath):
    """
    Test reading MIDI files with multilingual filenames to verify UTF-8 path support in Python bindings.
    """
    abs_path = os.path.abspath(filepath)
    print(f"[DEBUG] Current working directory: {os.getcwd()}")
    print(f"[DEBUG] Absolute path: {abs_path}")
    try:
        score = Score(abs_path)
        assert score.note_num() > 0 or len(score.tracks) > 0
    except Exception as e:
        pytest.fail(f"Failed to read file '{abs_path}': {e}")