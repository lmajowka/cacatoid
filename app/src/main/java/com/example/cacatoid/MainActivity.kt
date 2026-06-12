package com.example.cacatoid

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.widget.ArrayAdapter
import android.widget.AdapterView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.viewModels
import com.example.cacatoid.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val viewModel: SearchViewModel by viewModels()

    private val puzzles = listOf(71, 72, 73)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupPuzzleSpinner()
        setupButtons()
        observeViewModel()
    }

    private fun setupPuzzleSpinner() {
        val labels = puzzles.map { "Puzzle $it" }
        binding.puzzleSpinner.adapter = ArrayAdapter(
            this, android.R.layout.simple_spinner_dropdown_item, labels
        )
        binding.puzzleSpinner.setSelection(puzzles.indexOf(viewModel.selectedPuzzle))
        binding.puzzleSpinner.onItemSelectedListener =
            object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: android.view.View?, pos: Int, id: Long) {
                    viewModel.selectedPuzzle = puzzles[pos]
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
    }

    private fun setupButtons() {
        binding.startButton.setOnClickListener { viewModel.start() }
        binding.stopButton.setOnClickListener { viewModel.stop() }
        binding.copyButton.setOnClickListener { copyKey() }
    }

    private fun observeViewModel() {
        viewModel.running.observe(this) { running ->
            binding.startButton.isEnabled = !running
            binding.stopButton.isEnabled = running
            binding.puzzleSpinner.isEnabled = !running
            binding.statusText.text = getString(
                if (running) R.string.status_running else R.string.status_stopped
            )
        }

        viewModel.stats.observe(this) { s ->
            binding.currentKeyText.text =
                if (s.currentKeyHex.isEmpty()) getString(R.string.dash) else s.currentKeyHex
            binding.speedText.text = "Speed: ${formatNumber(s.keysPerSec)} keys/sec"
            binding.totalText.text = "Total checked: ${formatNumber(s.totalChecked)}"
        }

        viewModel.found.observe(this) { found ->
            if (found == null) {
                binding.resultPanel.visibility = android.view.View.GONE
            } else {
                binding.resultPanel.visibility = android.view.View.VISIBLE
                binding.resultText.text = buildString {
                    append("Puzzle:   ${found.puzzle}\n")
                    append("Address:  ${found.address}\n\n")
                    append("Priv hex: ${found.privKeyHex}\n\n")
                    append("WIF:      ${found.wif}")
                }
                binding.statusText.text = getString(R.string.match_found)
                Toast.makeText(this, R.string.match_found, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun copyKey() {
        val found = viewModel.found.value ?: return
        val clip = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clip.setPrimaryClip(ClipData.newPlainText("private key", found.privKeyHex))
        Toast.makeText(this, "Private key copied", Toast.LENGTH_SHORT).show()
    }

    private fun formatNumber(n: Long): String = "%,d".format(n)
}
